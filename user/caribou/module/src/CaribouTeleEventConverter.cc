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
  bool debug =false;

         //Go through planes
  for(int plane_id=0;plane_id<3;plane_id++){
    if(debug)
      std::cout << "*********************** at plane: " << plane_id<<std::endl;
    eudaq::StandardPlane plane(plane_id, "CaribouTele", "Alpide");
    plane.SetSizeZS(1024,512,0);

           // block is uint8_t
    auto block =  ev->GetBlock(plane_id);
    auto data = block.begin();
    if(debug)
      std::cout << "got a block " << std::dec<< block.size()<<std::endl;
    // //Retrieve event timestamp + trigger only from the first plane (7 bytes: 1000 0 <reserved[2:0]> <timestamp[47:0]>)
    // trigger ID identifier =0x88
    if(*data==0x88){
      uint32_t trigID =0x0;
      data++;
      for(uint i =0;i<4;++i){
        trigID +=(((uint32_t)(*data))<<(8*i)); // oder 4-i :shrug:
        data++;
      }
      if(debug)
        std::cout<< "TriggerID: " << trigID <<std::endl;
      d2->SetTriggerN(trigID);
    }
    if(*data==0x80){
      uint64_t time =0x0;
      data++;
      for(uint i =0;i<6;++i){
        time +=(((uint64_t)(*data))<<(8*i)); // oder 6-i :shrug:
        data++;
      }
      if(time >0xFFFFFFFF)
        time=0;
      if(debug)
        std::cout<< "Timestamp clk: " << std::hex<< time << std::dec;
      time *=25000;      // to ps

      d2->SetTimeBegin(time);
      d2->SetTimeEnd(time+25000);
      d2->SetTimestamp(time, time+25000, true);
      if(debug)
        std::cout << " and ps: " << time <<std::endl;
    }
    if(data == block.begin()){
      EUDAQ_WARN("No timestamp or trigger ID at begin of data block");
    }
    // dataword += 7;
    caribou::pearyRawData rawdata;
    rawdata.resize(sizeof(block[0]) * block.size() / sizeof(uintptr_t));
    std::memcpy(&rawdata[0], &block[0],
                sizeof(block[0]) * block.size());
    uint8_t region_header = 0xFF;
    auto dataword = rawdata.begin();

    while (dataword != rawdata.end()) {
      if(debug)
        std::cout << "Word: " << std::setfill('0') << std::setw(16) << std::hex <<(uint64_t)((*dataword)) <<std::endl;
      dataword++;
    }

    while(data!=block.end()){

             // idle/busy word
      if ((*data >> 4) == 0xF) {
        data++;
        if(debug)
          std::cout << "Idle" <<std::endl;
        continue;
      }
      // empty data block identifier
      else if ((*data >> 4) == 0xE) {
        if(debug)
          std::cout << "Empty" <<std::endl;
        break;
      } // data block with hits identifier
      else if ((*data >> 4) == 0xA) {
        if(debug)
          std::cout << "identifier" <<std::endl;
        data += 2;
      }// hits
      else if ((*data >> 5) == 0x6) {
        region_header = *data & 0x1F;
        if(debug)
          std::cout << "region header" <<std::endl;
        data++;
      } else if ((*data >> 6) == 0x1) {
        if(debug)
          std::cout << "data" <<std::endl;
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
        if(debug)
          std::cout << col <<  "\t" << row << std::endl;
        data++;
      } else if ((*data >> 4) == 0xB){
        data++;
        if(debug)
          std::cout << "Chip Trailer: " <<std::endl;

      } else{
        if(debug)
          std::cout << "Unknown tag: " << std::hex<<(int)*data<<std::endl;
        data++;
      }
    }
    if(debug)
      std::cout << plane.ID() <<"\t" << plane.PixVector().size()<<std::endl;
    d2->AddPlane(plane);
  }

  return true;
}
