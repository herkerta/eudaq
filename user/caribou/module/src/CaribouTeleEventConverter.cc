#include "CaribouEvent2StdEventConverter.hh"

using namespace eudaq;

namespace{
  auto dummy0 = eudaq::Factory<eudaq::StdEventConverter>::
    Register<CaribouTeleEventConverter>(CaribouTeleEventConverter::m_id_factory);
}

bool CaribouTeleEventConverter::Converting(eudaq::EventSPC d1, eudaq::StdEventSP d2, eudaq::ConfigSPC conf) const{
  auto ev = std::dynamic_pointer_cast<const eudaq::RawEvent>(d1);

  size_t nblocks= ev->NumBlocks();
  if(nblocks != 6) {
      if(!ev->IsBORE() && !ev->IsEORE()) {
        EUDAQ_ERROR("Wrong number of blocks, expecting 1, received " + std::to_string(nblocks));
      }
      EUDAQ_DEBUG("Empty event " + std::to_string(ev->GetEventNumber()) + (ev->IsBORE() ? " (BORE)" : (ev->IsEORE() ? " (EORE)" : "")));
      return false;
  }

  //Go through planes
  for(int plane_id=0;plane_id<6;plane_id++){
    eudaq::StandardPlane plane(plane_id, "CaribouTele", "Alpide");
    plane.SetSizeZS(1024,512,0);

    // block is uint8_t
    auto block =  ev->GetBlock(plane_id);

    //Retrieve event timestamp only from the first plane (7 bytes: 1000 0 <reserved[2:0]> <timestamp[47:0]>)
    if(plane_id==0){
        uint64_t ts = 0;
        for (int byte_cnt = 1; byte_cnt < 7; byte_cnt++) {
          ts += block[byte_cnt] << (6 - byte_cnt) * 8; // 40 MHz
        }
        ts *= 25000; // to ps
        d2->SetTimestamp(ts, ts, true);
    }

    auto dataword = block.begin();
    dataword += 7;

    uint8_t region_header = 0xFF;

    while (dataword != block.end()) {
        // idle/busy word
        if ((*dataword >> 4) == 0xF) {
          dataword++;
          continue;
        }
        // empty data block identifier
        if ((*dataword >> 4) == 0xE) {
          break;
        }
        // data block with hits identifier
        if ((*dataword >> 4) == 0xA) {
          dataword += 2;
        }
        // hits
        if ((*dataword >> 5) == 0x6) {
          region_header = *dataword & 0x1F;
        }
        if ((*dataword >> 6) == 0x1) {
          auto val = *dataword;
          uint8_t encoder_id = (val>>2) & 0xF;
          uint16_t addr = (val & 0x3)<<8;
          dataword++;
          auto val2 = *dataword;
          addr += val2;
          uint32_t col = region_header*32 + encoder_id*2;
          if(addr%4 == 1 || addr%4 ==2) col++;
          uint32_t row = addr/2;
          uint32_t lalala = 0;
          plane.PushPixel(col,row,0,lalala);
        }
    }
    d2->AddPlane(plane);
  }

  return true;
}
