#include "rtp/NoiseReductionHandler.h"

namespace erizo {


    DEFINE_LOGGER(NoiseReductionHandler, "rtp.NoiseReductionHandler");

    NoiseReductionHandler::NoiseReductionHandler(std::vector<std::string> parameters): parameters{parameters}{
        st = rnnoise_create(NULL);
    }

    NoiseReductionHandler::~NoiseReductionHandler(){
        rnnoise_destroy(st);
    }

    void NoiseReductionHandler::enable() {
    }

    void NoiseReductionHandler::disable() {
    }

    void NoiseReductionHandler::notifyUpdate() {

    }

    int NoiseReductionHandler::position(){
        return 1;
    }

    void NoiseReductionHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
           if (packet->type != AUDIO_PACKET){
               for(unsigned int i=0; i<3; i++){
                   memcpy(inBuffer, packet->data +(500*i), 500);
                   rnnoise_process_frame(st, inBuffer, outBuffer);
                   memcpy(packet->data, outBuffer + (500*i), 500);
               }
               memcpy(packet->data,outBuffer,1500);
           }
           ctx->fireRead(std::move(packet));
    }

    void NoiseReductionHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
            ctx->fireWrite(std::move(packet));
    }

}