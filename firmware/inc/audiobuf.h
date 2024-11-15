#pragma once

// Wrappers for interleaved audio buffers (InterleavingInputBuffer,
// InterleavingOutputBuffer) and the audio callback function that take care of
// interleaving and provide a range interface

namespace daisy2 {

template<typename T>
struct AudioSampleBase
{
    T left;
    T right;
};

/// @brief A stereo audio sample
using AudioSample = AudioSampleBase<float>;

template<typename SAMPLE>
using AudioInBufBase = std::span<const SAMPLE>;

/// @brief Stereo audio input buffer
/// @details This is a representation of InterleavingInputBuffer, as a span
using AudioInBuf = AudioInBufBase<AudioSample>;

template<typename SAMPLE>
using AudioOutBufBase = std::span<SAMPLE>;

/// @brief Stereo audio output buffer
/// @details This is a representation of InterleavingOutputBuffer, as a span
using AudioOutBuf = AudioOutBufBase<AudioSample>;

template<typename BUF, typename WRAP>
auto convertAudioBuf(BUF buf, size_t size)
{
    return WRAP(reinterpret_cast<WRAP::element_type*>(buf), size / 2);
}

/// @brief Convert InterleavingInputBuffer to AudioInBuf
/// @param buf 
/// @param size 
/// @return a AudioInBuf that refers to buf
AudioInBuf convertAudioInBuf(daisy::AudioHandle::InterleavingInputBuffer buf,
                             size_t size)
{
    return convertAudioBuf<daisy::AudioHandle::InterleavingInputBuffer,
                           AudioInBuf>(buf, size);
}

/// @brief Convert InterleavingOutputBuffer to AudioOutBuf
/// @param buf 
/// @param size 
/// @return a AudioOutBuf that refers to buf
AudioOutBuf convertAudioOutBuf(daisy::AudioHandle::InterleavingOutputBuffer buf,
                               size_t size)
{
    return convertAudioBuf<daisy::AudioHandle::InterleavingOutputBuffer,
                           AudioOutBuf>(buf, size);
}

using AudioCallback = void (*)(AudioInBuf inbuf, AudioOutBuf outbuf);

}
