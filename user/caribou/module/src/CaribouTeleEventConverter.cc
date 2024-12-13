#include "CaribouEvent2StdEventConverter.hh"


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
        for (int byte_cnt = 1; byte_cnt < 8; byte_cnt++) {
          ts += block[byte_cnt] << (6 - byte_cnt) * 8; // 40 MHz
        }
        ts *= 25000; // to ps
        d2->SetTimestamp(ts, ts, true);
    }

    auto dataword = block.begin();
    dataword += 8;

    uint8_t region_header = 0xFF;

    while (dataword != block.end()) {
        // idle/busy word
        if ((*dataword >> 4) == 0xF) {
          dataword++;
          continue;
        }
        // empty data block
        if ((*dataword >> 4) == 0xE) {
          d2->AddPlane(plane);
          break;
        }
        if ((*dataword >> 4) == 0xA) {
          dataword += 2;
        }
        if ((*dataword >> 5) == 0x6) {
          region_header = *dataword & 0x1F;
        }
        if ((*dataword >> 6) == 0x1) {
          auto val = *dataword;
          dataword++;
          auto val2 = *dataword;
         // plane.SetPixel(val, val2, ...);
        }

        // hit data identifier



    }
    //Skip over idle or busy words
//    int byte_cnt = 8;
//    uint8_t identifier = block[byte_cnt]>>4;
//    while(identifier==0xf){
//        byte_cnt++;
//        identifier = block[byte_cnt]>>4;
//    }
    //Next relevant word should be chip header (or empty frame header)
    //    if(identifier==0xe){
    //        d2->AddPlane(plane);
    //        continue;
    //    }
    //    else byte_cnt=byte_cnt+2;

    //    //Hit data
    //    uint8_t word = 0;
    //    uint8_t region_id = 0;
    //    while(byte_cnt<=block.size()){
    //        word = block[byte_cnt];
    //        if(word>>0xF){
    //            byte_cnt++;
    //            continue;
    //        }
    //        if(word>>5==0x6){
    //            region_id
    //        }

    }



//  for(uint bit = 0; bit < block.size();bit++){
//    auto  word = eudaq::getlittleendian<uint8_t>(&block[0]+bit);
//    // first entry == plane id
//    auto planeID = word;
//    bit++;
//    word = eudaq::getlittleendian<uint8_t>(&block[0]+bit);
//    uint16_t numHits = 0;
//    if(word>>7==1){
//    numHits = word <<0x8;
//    bit++;
//    word = eudaq::getlittleendian<uint8_t>(&block[0]+bit);
//    numHits+=word;
//    }
//    else{
//         numHits = word;
//    }

//    eudaq::StandardPlane plane(plane_id, "Alpide", "Alpide");
//    plane.SetSizeZS(1024,512,numHits);
//    for(auto hitcounter = 0; hitcounter < numHits; hitcounter++){
//        auto hit0 = eudaq::getlittleendian<uint8_t>(&block[0]+bit+1);
//        auto hit1 = eudaq::getlittleendian<uint8_t>(&block[0]+bit+2);
//        auto hit2 = eudaq::getlittleendian<uint8_t>(&block[0]+bit+3);
//        bit += 3;
//        uint32_t hit_encoded = (hit0<<14)+(hit1<<7)+hit2;
//        plane.SetPixel(hitcounter, hit_encoded >> 9, hit_encoded & 0x1FF,0);
//        //std::cout << std::hex << hit_encoded << " x =" << (int)(hit_encoded >> 9) << std::endl;
//    }
        d2->AddPlane(plane);
//  }
  }

  return true;
}
