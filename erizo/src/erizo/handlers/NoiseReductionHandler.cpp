#include "NoiseReductionHandler.h"
#include <vector>
#include <string>

namespace erizo {


    DEFINE_LOGGER(NoiseReductionHandler, "rtp.NoiseReductionHandler");

    NoiseReductionHandler::NoiseReductionHandler(std::vector<std::string> parameters): parameters{parameters}{
        sts = (DenoiseState**)malloc(1 * sizeof(DenoiseState *));
        model = rnnoise_get_model("orig");
        sts[0] = rnnoise_create(model);
        rnnoise_set_param(sts[0], RNNOISE_PARAM_SAMPLE_RATE, 8000);
        float reduction = pow(10, - std::stoi(parameters.at(1),nullptr,10)/10); //Atenunation to db
        ELOG_DEBUG("Param %f", reduction);
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

    Positions NoiseReductionHandler::position(){
        return Middle;
    }

    void NoiseReductionHandler::read(Context *ctx, std::shared_ptr<DataPacket> packet) {
           if (packet->type == AUDIO_PACKET){
               RtpHeader *head = reinterpret_cast<RtpHeader*> (packet->data);
               const int SIZE =  packet->length;
               const int HEADER_SIZE = head->getHeaderLength();
               for(int i = 0; i< SIZE - HEADER_SIZE; i++){
                   inBuffer[indexIn + i] = (float) ulaw2linear( (unsigned char)packet->data[i+HEADER_SIZE]);
               }
               indexIn += SIZE - HEADER_SIZE; //Copiamos los datos del paquete al array in
               packets.push_back(packet); //Guardamos el paquete
               while(indexIn >= 480){  //Si tenemos suficientes datos los mandamos a procesar
                   std::copy(inBuffer,inBuffer+480,processBuffer);
                   indexIn -= 480;
                   std::copy(inBuffer+480,std::end(inBuffer),inBuffer); //Sacamos 480 muestras y bajamos el index
                   rnnoise_process_frame(sts[0], processBuffer, processBuffer);
                   std::copy(processBuffer,processBuffer + 480, outBuffer + indexOut); //Procesamos y sacamos
                   indexOut += 480;
               }
               if(indexOut>= SIZE - HEADER_SIZE) {
                   int bytes = 0;
                   int packetsSend = 0;
                   for (std::shared_ptr <DataPacket> packet : packets) {//Mandamos paquetes
                       if (bytes + SIZE - HEADER_SIZE <= indexOut) { //Si tenemos todas sus muestras procesadas
                           for (int i = 0; i < SIZE - HEADER_SIZE; i++) { //Copiamos la nueva info del paquete
                               packet->data[i + HEADER_SIZE] = (char) lin2ulaw((short)outBuffer[i + bytes]);
                           }
                           bytes += SIZE - HEADER_SIZE;
                           ctx->fireRead(std::move(packet));
                           packetsSend++;
                       }
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

    //G711 code and decode. Rnnoise requires PCM 16bits so we have to decode
    #define	SIGN_BIT	(0x80)		/* Sign bit for a A-law byte. */
    #define	QUANT_MASK	(0xf)		/* Quantization field mask. */
    #define	NSEGS		(8)		/* Number of A-law segments. */
    #define	SEG_SHIFT	(4)		/* Left shift for segment number. */
    #define	SEG_MASK	(0x70)		/* Segment field mask. */
    #define	BIAS		(0x84)
    #define CLIP            8159
    /*
     * ulaw2linear() - Convert a u-law value to 16-bit linear PCM
     *
     * First, a biased linear code is derived from the code word. An unbiased
     * output can then be obtained by subtracting 33 from the biased code.
     *
     * Note that this function expects to be passed the complement of the
     * original code word. This is in keeping with ISDN conventions.
     */
    short NoiseReductionHandler::ulaw2linear(unsigned char	u_val){
        short t;

        /* Complement to obtain normal u-law value. */
        u_val = ~u_val;

        /*
         * Extract and bias the quantization bits. Then
         * shift up by the segment number and subtract out the bias.
         */
        t = ((u_val & QUANT_MASK) << 3) + BIAS;
        t <<= ((unsigned)u_val & SEG_MASK) >> SEG_SHIFT;

        return ((u_val & SIGN_BIT) ? (BIAS - t) : (t - BIAS));
    }




    unsigned char NoiseReductionHandler::lin2ulaw(short pcm_val)	/* 2's complement (16-bit range) */
    {
        short		mask;
        short		seg;
        unsigned char	uval;

        /* Get the sign and the magnitude of the value. */
        pcm_val = pcm_val >> 2;
        if (pcm_val < 0) {
            pcm_val = -pcm_val;
            mask = 0x7F;
        } else {
            mask = 0xFF;
        }
        if ( pcm_val > CLIP ) pcm_val = CLIP;		/* clip the magnitude */
        pcm_val += (BIAS >> 2);

        /* Convert the scaled magnitude to segment number. */
        seg = search(pcm_val, seg_uend, 8);

        /*
         * Combine the sign, segment, quantization bits;
         * and complement the code word.
         */
        if (seg >= 8)		/* out of range, return maximum value. */
            return (unsigned char) (0x7F ^ mask);
        else {
            uval = (unsigned char) (seg << 4) | ((pcm_val >> (seg + 1)) & 0xF);
            return (uval ^ mask);
        }

    }

    short NoiseReductionHandler::search(
            short		val,
            short		*table,
            short		size)
    {
        short		i;

        for (i = 0; i < size; i++) {
            if (val <= *table++)
                return (i);
        }
        return (size);
    }

}
