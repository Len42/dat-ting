#pragma once

/// @brief Abstract base class for animations
class Animation
{
public:
    /// @brief Initialize the animation
    virtual void Init() = 0;

    /// @brief Display the given step (frame) of the animation
    /// @param step Step number
	/// @return true if the animation should continue running, false if it is finished
    virtual bool Step(unsigned step) = 0;
};

/// @brief Animation runner
/// @details Runs a single animation. An instance of this class is used by
/// @ref AnimationTask.
class Animator
{
public:
	/// @brief Start running the given @ref Animation
    /// @details @ref AnimationTask will actually run the animation by calling
    /// Step().
	/// @param animation 
	void Start(Animation* animation)
    {
        fRunning = false;
        currentAnimation = animation;
        if (currentAnimation) {
            currentAnimation->Init();
            fRunning = true;
        } else {
            fRunning = false;
        }
        step = 0;
    }

    /// @brief Stop running the current animation
    void Stop() { fRunning = false; }

	/// @brief Display the next step (frame) of the currently-running @ref Animation
    /// @details Does nothing if no animation is running. Checks the value
    /// returned by @ref Animation::Step to see if the animation is finished.
	/// @return true if the animation should continue running, false if it is finished
	bool Step()
    {
        if (fRunning) {
            return (fRunning = currentAnimation->Step(step++));
        } else {
            return false;
        }
    }

    /// @brief Check if an animation is running
    /// @return 
    bool IsRunning() const { return fRunning; }

    /// @brief Animation frame rate - nominally 20 fps
    static constexpr unsigned framePeriodUs = 50'000;

protected:
    bool fRunning = false;
    Animation* currentAnimation = nullptr;
	unsigned step = 0;
};

/// @brief @ref tasks::Task to display the currently-running animation, if any
class AnimationTask : public tasks::Task<AnimationTask>
{
public:
    unsigned intervalMicros() const { return Animator::framePeriodUs; }

    void init() { }

    void execute() { StepAnim(); }

public:
	/// @brief Start displaying an animation
	/// @param animation 
	static void StartAnim(Animation* animation) { animator.Start(animation); }

    /// @brief Stop displaying the current animation
    static void StopAnim() { animator.Stop(); }

	/// @brief Display the next step (frame) of the current animation
	/// @return 
	static bool StepAnim() { return animator.Step(); }

protected:
    static inline Animator animator;
};

/// @brief Animation sequence
/// @details This is an animation that executes several sub-animations in sequence.
/// It ends when all the sub-animations have run to completion.
/// @tparam ...ANIMS A list of @ref Animation subclasses which are run in order
template<typename... ANIMS>
class AnimationSeq : public Animation
{
public:
    consteval AnimationSeq()
    {
        int i = 0;
        ((animations[i++] = &instance<ANIMS>), ...);
    }

	void Init() override
	{
        InitCurrent();
	}

	/// @brief Execute a step of the current animation. If it's finished, start
    /// the next animation in the sequence.
	/// @param step 
	/// @return 
	bool Step(unsigned step) override
	{
        if (curAnim != std::end(animations)) {
            if (!animator.Step()) {
                if (++curAnim == std::end(animations)) {
                    return false;
                } else {
                    InitCurrent();
                }
            }
        }
        return true;
	}

protected:
    void InitCurrent()
    {
        if (curAnim != std::end(animations)) {
            animator.Start(*curAnim);
        }
    }

protected:
    Animator animator;

    Animation* animations[sizeof...(ANIMS)];

    using IterType = decltype(std::begin(animations));
    IterType curAnim = std::begin(animations);

    unsigned curAnimStep = 0;

    template<typename ANIM>
    static inline ANIM instance;
};

/// @brief Animation to show the output amplitude of one or both audio channels
/// @tparam NUM Number of audio channels
template<unsigned NUM>
class AnimAmplitude : public Animation
{
public:
    void Init() override
    {
        xSpace = HW::display.Width() / numChannels;
        yPos = HW::display.Height() / 2;
        //maxRadius = xSpace / 2 - 1;
        maxRadius = HW::display.Width() / 4 - 1;
        lastSample = { };
        recentSamples.clear();
    }

    /// @brief Update the animation using the samples from the last several
    /// animation updates, including lastSample which was set by SetAmplitude().
    /// @param step 
    /// @return 
    bool Step(unsigned step) override
    {
        recentSamples.push(lastSample);
        lastSample = { }; // reset max sample value for next time
        HW::display.Fill(false);
        for (auto&& sample : recentSamples) {
            unsigned xPos = xSpace / 2;
            for (unsigned i = 0; i < numChannels; ++i) {
                unsigned rad = std::sqrt(sample[i]) * maxRadius;
                if (rad > 1) {
                    HW::display.DrawCircle(xPos, yPos, rad, true);
                }
                xPos += xSpace;
            }
        }
        HW::display.Update();
        // never stop
        return true;
    }

    /// @brief Save an audio sample to be used in the next animation update
    /// @details This may be called many times (e.g. from AudioCallback) but the
    /// value won't be used until the next animation update.
    /// @param samp A stereo audio sample
    void SetAmplitude(daisy2::AudioSample samp)
    {
        SetAmplitude(samp.left, samp.right);
    }

    /// @brief Save an audio sample to be used in the next animation update
    /// @details This may be called many times (e.g. from AudioCallback) but the
    /// value won't be used until the next animation update.
    /// @param ...ampls The amplitude(s) of an audio sample - any number of channels
    void SetAmplitude(std::convertible_to<float> auto... ampls)
    {
        static_assert(sizeof...(ampls) <= numChannels);
        int i = 0;
        // Use max for nicer animation
        (((lastSample[i] = std::max(lastSample[i], std::abs(ampls))), ++i), ...);
        // that's C++, baby!
    }

protected:
    static constexpr unsigned numChannels = NUM;
    unsigned xSpace = 0;
    unsigned yPos = 0;
    unsigned maxRadius = 0;

    using Sample = std::array<float, numChannels>;

    Sample lastSample = { };

    static constexpr size_t numCircles = 3;
    RingBuf<Sample, numCircles> recentSamples;
};
