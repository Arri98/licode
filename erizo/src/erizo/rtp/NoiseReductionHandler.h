//
// Created by lic on 25/11/20.
//

#ifndef ERIZO_SRC_ERIZO_RTP_NOISEREDUCTIONHANDLER_H_
#define ERIZO_SRC_ERIZO_RTP_NOISEREDUCTIONHANDLER_H_


#include "pipeline/Handler.h"
#include "./logger.h"
#include "../MediaDefinitions.h"

extern "C"{
#include "rnnoise-nu/include/rnnoise-nu.h"
}
#define FRAME_SIZE 480;

namespace erizo {

    class MediaStream;

    class NoiseReductionHandler : public CustomHandler {
        DECLARE_LOGGER();

    public:
        NoiseReductionHandler(std::vector<std::string> parameters);
        ~NoiseReductionHandler();

        void enable() override;
        void disable() override;

        std::string getName() override {
            return "slideshow";
        }

        void read(Context *ctx, std::shared_ptr<DataPacket> packet) override;
        void write(Context *ctx, std::shared_ptr<DataPacket> packet) override;
        void notifyUpdate() override;

        int position () override;

    private:
        std::vector<std::string> parameters;
        DenoiseState **sts;
        RNNModel *model = NULL;
        float inBuffer[3000];
        float processBuffer[480];
        float outBuffer[3000];
        int indexIn = 0;
        int indexOut = 0;
        std::vector<std::shared_ptr<DataPacket>> packets = {};
    };

}
#endif  // ERIZO_SRC_ERIZO_RTP_LOWERFPSHANDLER_H_
