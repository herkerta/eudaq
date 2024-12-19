#include "CaribouEvent2StdEventConverter.hh"

using namespace eudaq;

namespace{
  auto dummy0 = eudaq::Factory<eudaq::StdEventConverter>::
      Register<CaribouTeleEventConverter>(CaribouTeleEventConverter::m_id_factory);
}

bool CaribouTeleEventConverter::Converting(eudaq::EventSPC d1, eudaq::StdEventSP d2, eudaq::ConfigSPC conf) const{
  auto ev = std::dynamic_pointer_cast<const eudaq::RawEvent>(d1);

  if(ev->IsBORE())
    return true;

  d2->SetDetectorType("ALPIDE");


  size_t nblocks= ev->NumBlocks();
  if(nblocks != 3) {
    if(!ev->IsBORE() && !ev->IsEORE()) {
      EUDAQ_ERROR("Wrong number of blocks, expecting 1, received " + std::to_string(nblocks));
    }
    EUDAQ_DEBUG("Empty event " + std::to_string(ev->GetEventNumber()) + (ev->IsBORE() ? " (BORE)" : (ev->IsEORE() ? " (EORE)" : "")));
    return false;
  }
  //Go through planes
  for(int plane_id=0;plane_id<3;plane_id++){
    eudaq::StandardPlane plane(plane_id, "CaribouTele", "Alpide");
    plane.SetSizeZS(1024,512,0);

    // block is uint8_t
    auto block =  ev->GetBlock(plane_id);
    auto data = block.begin();
    // //Retrieve event timestamp + trigger only from the first plane (7 bytes: 1000 0 <reserved[2:0]> <timestamp[47:0]>)
    // trigger ID identifier =0x88
    uint32_t trigID =0x0;
    if(*data==0x88){
      data++;
      for(uint i =0;i<4;++i){
        trigID +=(((uint32_t)(*data))<<(8*i)); // oder 4-i :shrug:
        data++;
      }
      d2->SetTriggerN(trigID);
    }
    if(*data==0x80){
      uint64_t time =0x0;
      data++;
      for(uint i =0;i<6;++i){
        time +=(((uint64_t)(*data))<<(8*i)); // oder 6-i :shrug:
        data++;
      }
      // catch t_0 event with max time stamp
      if(time ==0xFFFFFFFFFFFF){
        if(trigID==0){
        time=0;
        } else EUDAQ_THROW("Maximal timestamp at trigger ID " + std::to_string(trigID));
      }
      time *=25000;      // to ps
      d2->SetTimeBegin(time);
      d2->SetTimeEnd(time);
      d2->SetTimestamp(time, time, true);
    }
    if(data == block.begin()){
      EUDAQ_ERROR("No timestamp or trigger ID at begin of data block");
    }
    // reading hit data block
    while(data!=block.end()){

             // idle/busy word
      if ((*data >> 4) == 0xF) {
        data++;
        continue;
      }
      // empty data block identifier
      else if ((*data >> 4) == 0xE) {
        break;
      } // data block with hits identifier
      else if ((*data >> 4) == 0xA) {
        data += 2;
      }// regoion header
      else if ((*data >> 5) == 0x6) {
        region_header = *data & 0x1F;
        data++;
      } // real hit data
      else if ((*data >> 6) == 0x1) {
        auto val = *data;
        uint8_t encoder_id = (val>>2) & 0xF;
        uint16_t addr = (val & 0x3)<<8;
        data++;
        auto val2 = *data;
        addr += val2;
        uint32_t col = region_header*32 + encoder_id*2;
        if(addr%4 == 1 || addr%4 ==2) col++;
        uint32_t row = addr/2;
        uint32_t lalala = 0;
        plane.PushPixel(col,row,0,lalala);
        data++;
      } // chip trailer
      else if ((*data >> 4) == 0xB){
        data++;
      } else{
        EUDAQ_ERROR("Unknown data package"+std::to_string((int)(*data)));
        data++;
      }
    }
    d2->AddPlane(plane);
  }
  return true;
}
