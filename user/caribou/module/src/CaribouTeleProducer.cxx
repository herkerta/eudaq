#include "eudaq/Producer.hh"
#include "eudaq/Configuration.hh"

#include "peary/device/DeviceManager.hpp"
#include "peary/utils/configuration.hpp"
#include "peary/utils/log.hpp"

#include <vector>
#include <thread>

using namespace caribou;

class CaribouTeleProducer : public eudaq::Producer {
public:
  CaribouTeleProducer(const std::string name, const std::string &runcontrol);
  ~CaribouTeleProducer() override;
  void DoInitialise() override;
  void DoConfigure() override;
  void DoStartRun() override;
  void DoStopRun() override;
  void DoReset() override;
  // void DoTerminate() override;
  void RunLoop() override;

  static const uint32_t m_id_factory = eudaq::cstr2hash("CaribouTeleProducer");
private:
  unsigned m_ev;

  DeviceManager* manager_;
  std::string name_;

  std::mutex device_mutex_;
  LogLevel level_;
  bool m_exit_of_run;

  size_t number_of_subevents_{0};
};

namespace{
  auto dummy0 = eudaq::Factory<eudaq::Producer>::
    Register<CaribouTeleProducer, const std::string&, const std::string&>(CaribouTeleProducer::m_id_factory);
}

CaribouTeleProducer::CaribouTeleProducer(const std::string name, const std::string &runcontrol)
: eudaq::Producer(name, runcontrol), m_ev(0), m_exit_of_run(false) {
  // Add cout as the default logging stream
  Log::addStream(std::cout);

  LOG(INFO) << "Instantiated CaribouTeleProducer with name \"" << name << "\"";

  // Create new Peary device manager
  manager_ = new DeviceManager();
}

CaribouTeleProducer::~CaribouTeleProducer() {
    delete manager_;
}

void CaribouTeleProducer::DoReset() {
  LOG(WARNING) << "Resetting CaribouProducer";
  m_exit_of_run = true;

  // Delete all devices:
  std::lock_guard<std::mutex> lock{device_mutex_};
  manager_->clearDevices();
}

void CaribouTeleProducer::DoInitialise() {
  LOG(INFO) << "Initialising CaribouProducer";
  auto ini = GetInitConfiguration();

  auto level = ini->Get("log_level", "INFO");
  try {
    LogLevel log_level = Log::getLevelFromString(level);
    Log::setReportingLevel(log_level);
    LOG(INFO) << "Set verbosity level to \"" << std::string(level) << "\"";
  } catch(std::invalid_argument& e) {
    LOG(ERROR) << "Invalid verbosity level \"" << std::string(level) << "\", ignoring.";
  }

  level_ = Log::getReportingLevel();

  // Open configuration file and create object:
  caribou::ConfigParser cfg;
  auto confname = ini->Get("config_file", "");
  std::ifstream file(confname);
  EUDAQ_INFO("Attempting to use initial device configuration \"" + confname + "\"");
  if(!file.is_open()) {
    LOG(ERROR) << "No configuration file provided.";
    EUDAQ_ERROR("No Caribou configuration file provided.");
  } else {
    cfg = caribou::ConfigParser(file);
  }

  std::lock_guard<std::mutex> lock{device_mutex_};
  // Loop over all devices in the config:
  for(const auto [name, config] : cfg.GetAllConfigs()) {
    const auto type = config.Get<std::string>("type");
    const auto device_id = manager_->addDevice(type, config);
    EUDAQ_INFO("Manager returned device ID " + std::to_string(device_id) + " for device " + name);
  }
}

// This gets called whenever the DAQ is configured
void CaribouTeleProducer::DoConfigure() {
  auto config = GetConfiguration();
  LOG(INFO) << "Configuring CaribouTeleProducer: " << config->Name();

  std::lock_guard<std::mutex> lock{device_mutex_};
  for(auto device : manager_->getDevices()) {
      EUDAQ_INFO("Configuring device " + device->getName());

      // Switch on the device power:
      device->powerOn();

      // Wait for power to stabilize and for the TLU clock to be present
      eudaq::mSleep(1000);

      // Configure the device
      device->configure();

      // Set additional registers from the configuration:

      if(config->Has("register_key") || config->Has("register_value")) {
          auto key = config->Get("register_key", "");
          auto value = config->Get("register_value", 0);
          device->setRegister(key, value);
          EUDAQ_USER("Setting " + key + " = " + std::to_string(value));
      }
  }

  // Allow to stack multiple sub-events
  number_of_subevents_ = config->Get("number_of_subevents", 6);
  EUDAQ_USER("Will stack " + std::to_string(number_of_subevents_) + " subevents before sending event");

  LOG(STATUS) << "CaribouProducer configured. Ready to start run.";
}

void CaribouTeleProducer::DoStartRun() {
  m_ev = 0;

  LOG(INFO) << "Starting run...";

  // Start the DAQ
  std::lock_guard<std::mutex> lock{device_mutex_};

  // Sending initial Begin-of-run event, just containing tags with detector information:
  // Create new event
  auto event = eudaq::Event::MakeUnique("CaribouTeleEvent");
  event->SetBORE();

  // Use software and firmware version from first device:
  for(auto device : manager_->getDevices()) {
    event->SetTag("software",  device->getVersion());
    event->SetTag("firmware",  device->getFirmwareVersion());
    event->SetTag("timestamp", LOGTIME);

    break;
  }

  // FIXME we would like to write all registers, let's see how to do that with naming...
  // auto registers = device_->getRegisters();
  //  for(const auto& reg : registers) {
  //    event->SetTag(reg.first, reg.second);
  //  }

  // Send the event to the Data Collector
  SendEvent(std::move(event));

  // Start DAQ:
  for(auto device : manager_->getDevices()) {
    device->daqStart();
  }

  LOG(INFO) << "Started run.";
  m_exit_of_run = false;
}

void CaribouTeleProducer::DoStopRun() {

  LOG(INFO) << "Draining buffers...";
  eudaq::mSleep(500);
  LOG(INFO) << "Stopping run...";

  // Set a flag to signal to the polling loop that the run is over
  m_exit_of_run = true;

  // Stop the DAQ
  std::lock_guard<std::mutex> lock{device_mutex_};
  for(auto device : manager_->getDevices()) {
    device->daqStop();
  }
  LOG(INFO) << "Stopped run.";
}

void CaribouTeleProducer::RunLoop() {

  Log::setReportingLevel(level_);

  LOG(INFO) << "Starting run loop...";
  std::lock_guard<std::mutex> lock{device_mutex_};

  std::vector<eudaq::EventSPC> data_buffer;
  while(!m_exit_of_run) {
    for(int plane_id=0; plane_id<6; plane_id++){
      auto device = manager_->getDevice(plane_id);
      try {
          // Retrieve data from the device:
          auto data = device->getRawData();

          if(!data.empty()) {
              // Create new event
              auto event = eudaq::Event::MakeUnique("CaribouPLANE" + std::to_string(plane_id) + "Event");
              // Set event ID
              event->SetEventN(m_ev);
              // Add data to the event
              event->AddBlock(0, data);

              if(number_of_subevents_ == 0) {
                  // We do not want to generate sub-events - send the event directly off to the Data Collector
                  SendEvent(std::move(event));
              } else {
                  // We are still buffering sub-events, buffer not filled yet:
                  data_buffer.push_back(std::move(event));
              }
          }

          // Buffer of sub-events is full, let's ship this off to the Data Collector
          if(!data_buffer.empty() && data_buffer.size() == number_of_subevents_) {
              auto evup = eudaq::Event::MakeUnique("CaribouTeleEvent");
              for(auto& subevt : data_buffer) {
                  evup->AddSubEvent(subevt);
              }
              SendEvent(std::move(evup));
              data_buffer.clear();
              m_ev++;
          }

      } catch(caribou::NoDataAvailable&) {
          continue;
      } catch(caribou::DataException& e) {
          // Retrieval failed, retry once more before aborting:
          EUDAQ_WARN(std::string(e.what()) + ", skipping data packet");
          continue;
      } catch(caribou::caribouException& e) {
          EUDAQ_ERROR(e.what());
          break;
      }
    }
  }
  // Send remaining pixel data:
  if(!data_buffer.empty()) {
    LOG(INFO) << "Sending remaining " << data_buffer.size() << " events from data buffer";
    auto evup = eudaq::Event::MakeUnique("CaribouTeleEvent");
    for(auto& subevt : data_buffer) {
      evup->AddSubEvent(subevt);
    }
    SendEvent(std::move(evup));
    data_buffer.clear();
  }

  LOG(INFO) << "Exiting run loop.";
}
