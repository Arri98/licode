#include "rtp/NoiseReductionHandler.h"
#include <vector>
#include <string>

namespace erizo {


    DEFINE_LOGGER(NoiseReductionHandler, "rtp.NoiseReductionHandler");

    NoiseReductionHandler::NoiseReductionHandler(std::vector<std::string> parameters): parameters{parameters}{
        sts = (DenoiseState**)malloc(1 * sizeof(DenoiseState *));
        model = rnnoise_get_model("orig");
        sts[0] = rnnoise_create(model);
        rnnoise_set_param(sts[0], RNNOISE_PARAM_SAMPLE_RATE, 8000);
        int reduction = std::stoi(parameters.at(1),nullptr,10);
        ELOG_DEBUG("Param %d", reduction);
        rnnoise_set_param(sts[0], RNNOISE_PARAM_MAX_ATTENUATION, reduction);
    }

    NoiseReductionHandler::~NoiseReductionHandler(){
        rnnoise_destroy(sts[0]);
        free(sts);
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
           if (packet->type == AUDIO_PACKET){
               const int HEADER_SIZE = packet->length;
               for(int i = 0; i< 1500 - HEADER_SIZE; i++){
                   inBuffer[indexIn + i] = (float) packet->data[i+HEADER_SIZE];
               }
               indexIn += 1500 - HEADER_SIZE; //Copiamos los datos del paquete al array in
               packets.push_back(packet); //Guardamos el paquete
               while(indexIn >= 480){  //Si tenemos suficientes datos los mandamos a procesar
                   std::copy(inBuffer,inBuffer+480,processBuffer);
                   indexIn -= 480;
                   std::copy(inBuffer+480,std::end(inBuffer),inBuffer); //Sacamos 480 muestras y bajamos el index
                   rnnoise_process_frame(sts[0], processBuffer, processBuffer);
                   std::copy(processBuffer,processBuffer + 480, outBuffer + indexOut); //Procesamos y sacamos
                   indexOut += 480;
                   //ELOG_DEBUG("Index in: %d", indexIn);
                   //ELOG_DEBUG("Index out: %d", indexOut);
               }
               if(indexOut>= 1500 - HEADER_SIZE) {
                   int bytes = 0;
                   int packetsSend = 0;
                   for (std::shared_ptr <DataPacket> packet : packets) {//Mandamos paquetes
                       if (bytes + 1500 - HEADER_SIZE <= indexOut) { //Si tenemos todas sus muestras procesadas
                           for (int i = 0; i < 1500 - HEADER_SIZE; i++) { //Copiamos la nueva info del paquete
                               packet->data[i + HEADER_SIZE] = (char) outBuffer[i + bytes];
                           }
                           bytes += 1500 - HEADER_SIZE;
                           ctx->fireRead(std::move(packet));
                           packetsSend++;
                       }
                       //ELOG_DEBUG("Packets out: %d", packetsSend);
                       //ELOG_DEBUG("Bytes send: %d", bytes);
                   }
                   packets.erase(packets.begin(),packets.begin()+packetsSend);
                   std::copy(outBuffer + bytes, std::end(outBuffer), outBuffer);
                   indexOut -= bytes; //Bajamos el indice
               }

           }else {
               ctx->fireRead(std::move(packet));
           }
    }

    void NoiseReductionHandler::write(Context *ctx, std::shared_ptr<DataPacket> packet) {
            ctx->fireWrite(std::move(packet));
    }

}