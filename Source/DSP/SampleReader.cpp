#include "SampleReader.h"
#include "CircularBuffer.h"

namespace sk4n {

void SampleReader::prepare (double sampleRate)
{
    aaLp.setSampleRate (sampleRate);
    aaLp.setCutoff (18000.0);
    aaLp.reset();
}

void SampleReader::reset()
{
    aaLp.reset();
}

void SampleReader::setAntiAliasCutoff (double hz)
{
    aaLp.setCutoff (hz);
}

float SampleReader::read (const CircularBuffer& buf,
                          int   writeIndex,
                          float positionSamples,
                          float oscPhase,
                          float window01,
                          float ampWindow)
{
    const int active = buf.getActiveSize();
    if (active <= 0) return 0.0f;

    const float windowSamples = window01 * (static_cast<float> (active) * 0.25f);
    const float readPos = static_cast<float> (writeIndex)
                        - positionSamples
                        + windowSamples * oscPhase;

    const float raw = buf.readCubic (readPos);
    const float aa  = aaLp.process (raw);
    return aa * ampWindow;
}

} // namespace sk4n
