#include "MainComponent.h"

namespace
{
const juce::Colour backgroundColour { 0xff141719 };
const juce::Colour panelColour { 0xff1d2224 };
const juce::Colour borderColour { 0xff343b3e };
const juce::Colour textColour { 0xffe6e9e7 };
const juce::Colour mutedTextColour { 0xff899396 };
const juce::Colour accentColour { 0xffd9f36a };
constexpr int trackRowHeight = 88;

void styleButton(juce::TextButton& button, juce::Colour fill = panelColour)
{
    button.setColour(juce::TextButton::buttonColourId, fill);
    button.setColour(juce::TextButton::buttonOnColourId, accentColour);
    button.setColour(juce::TextButton::textColourOffId, textColour);
    button.setColour(juce::TextButton::textColourOnId, backgroundColour);
}
}

MainComponent::MainComponent()
    : sampleModel(*this), sampleList("Samples", &sampleModel), arrangement(*this)
{
    formatManager.registerBasicFormats();
    pluginFormats.addDefaultFormats();

    setOpaque(true);
    setSize(1440, 900);
    setAudioChannels(0, 2);

    titleLabel.setText("DAWSOFTWARE", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(19.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, textColour);
    addAndMakeVisible(titleLabel);

    timeLabel.setText("00:00.000", juce::dontSendNotification);
    timeLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    timeLabel.setColour(juce::Label::textColourId, accentColour);
    timeLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(timeLabel);

    styleButton(playButton, accentColour);
    playButton.setColour(juce::TextButton::textColourOffId, backgroundColour);
    playButton.onClick = [this]
    {
        const juce::ScopedLock lock(audioLock);
        if (tracks.empty())
            return;
        if (playing.exchange(true))
            return;
        for (auto& track : tracks)
        {
            if (track->transport.getCurrentPosition() >= track->durationSeconds)
                track->transport.setPosition(0.0);
            track->transport.start();
        }
    };
    addAndMakeVisible(playButton);

    styleButton(stopButton);
    stopButton.onClick = [this]
    {
        const juce::ScopedLock lock(audioLock);
        playing = false;
        for (auto& track : tracks)
        {
            track->transport.stop();
            track->transport.setPosition(0.0);
        }
        playheadSeconds = 0.0;
        timeLabel.setText("00:00.000", juce::dontSendNotification);
        arrangement.repaint();
    };
    addAndMakeVisible(stopButton);

    libraryLabel.setText("SAMPLE LIBRARY", juce::dontSendNotification);
    libraryLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    libraryLabel.setColour(juce::Label::textColourId, mutedTextColour);
    addAndMakeVisible(libraryLabel);

    styleButton(importButton);
    importButton.onClick = [this] { importLibraryFolder(); };
    addAndMakeVisible(importButton);

    sampleList.setRowHeight(34);
    sampleList.setColour(juce::ListBox::backgroundColourId, panelColour);
    sampleList.setColour(juce::ListBox::outlineColourId, borderColour);
    sampleList.setOutlineThickness(1);
    addAndMakeVisible(sampleList);

    styleButton(addSampleButton);
    addSampleButton.onClick = [this]
    {
        const auto row = sampleList.getSelectedRow();
        const auto file = sampleModel.fileForRow(row);
        if (file.existsAsFile())
            addSampleToArrangement(file);
    };
    addAndMakeVisible(addSampleButton);

    arrangement.setOpaque(true);
    addAndMakeVisible(arrangement);

    inspectorLabel.setText("TRACK INSPECTOR", juce::dontSendNotification);
    inspectorLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    inspectorLabel.setColour(juce::Label::textColourId, mutedTextColour);
    addAndMakeVisible(inspectorLabel);

    trackNameLabel.setText("No track selected", juce::dontSendNotification);
    trackNameLabel.setColour(juce::Label::textColourId, textColour);
    trackNameLabel.setFont(juce::Font(15.0f, juce::Font::bold));
    addAndMakeVisible(trackNameLabel);

    gainValueLabel.setText("LEVEL     -1.9 dB", juce::dontSendNotification);
    gainValueLabel.setColour(juce::Label::textColourId, mutedTextColour);
    gainValueLabel.setFont(juce::Font(11.0f, juce::Font::bold));
    addAndMakeVisible(gainValueLabel);

    gainSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    gainSlider.setRange(0.0, 1.5, 0.01);
    gainSlider.setValue(0.8, juce::dontSendNotification);
    gainSlider.setColour(juce::Slider::trackColourId, accentColour);
    gainSlider.setColour(juce::Slider::thumbColourId, textColour);
    gainSlider.setColour(juce::Slider::backgroundColourId, borderColour);
    gainSlider.onValueChange = [this]
    {
        if (selectedTrack < 0 || selectedTrack >= static_cast<int>(tracks.size()))
            return;
        tracks[static_cast<size_t>(selectedTrack)]->gain = static_cast<float>(gainSlider.getValue());
        const auto decibels = juce::Decibels::gainToDecibels(gainSlider.getValue(), -100.0);
        gainValueLabel.setText("LEVEL     " + juce::String(decibels, 1) + " dB", juce::dontSendNotification);
    };
    addAndMakeVisible(gainSlider);

    muteButton.setColour(juce::ToggleButton::textColourId, textColour);
    muteButton.onClick = [this]
    {
        if (selectedTrack >= 0 && selectedTrack < static_cast<int>(tracks.size()))
            tracks[static_cast<size_t>(selectedTrack)]->muted = muteButton.getToggleState();
    };
    addAndMakeVisible(muteButton);

    effectLabel.setText("EFFECTS", juce::dontSendNotification);
    effectLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    effectLabel.setColour(juce::Label::textColourId, mutedTextColour);
    addAndMakeVisible(effectLabel);

    styleButton(loadPluginButton);
    loadPluginButton.onClick = [this] { loadVst3(); };
    addAndMakeVisible(loadPluginButton);

    refreshLibrary();
    startTimerHz(30);
}

MainComponent::~MainComponent()
{
    shutdownAudio();
    const juce::ScopedLock lock(audioLock);
    for (auto& track : tracks)
        track->transport.setSource(nullptr);
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    const juce::ScopedLock lock(audioLock);
    for (auto& track : tracks)
    {
        track->scratch.setSize(2, samplesPerBlockExpected);
        if (track->effect)
        {
            track->effect->setPlayConfigDetails(2, 2, sampleRate, samplesPerBlockExpected);
            track->effect->prepareToPlay(sampleRate, samplesPerBlockExpected);
        }
    }
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& output)
{
    output.clearActiveBufferRegion();
    const juce::ScopedLock lock(audioLock);
    juce::MidiBuffer midi;

    for (auto& track : tracks)
    {
        if (track->muted.load() || !track->readerSource)
            continue;

        track->scratch.setSize(2, output.numSamples, false, false, true);
        track->scratch.clear();
        juce::AudioSourceChannelInfo sourceInfo(&track->scratch, 0, output.numSamples);
        track->transport.getNextAudioBlock(sourceInfo);

        if (track->effect)
            track->effect->processBlock(track->scratch, midi);

        const auto gain = track->gain.load();
        for (int channel = 0; channel < output.buffer->getNumChannels(); ++channel)
        {
            const auto sourceChannel = juce::jmin(channel, track->scratch.getNumChannels() - 1);
            output.buffer->addFrom(channel, output.startSample, track->scratch, sourceChannel, 0,
                                   output.numSamples, gain);
        }
    }
}

void MainComponent::releaseResources()
{
    const juce::ScopedLock lock(audioLock);
    for (auto& track : tracks)
        if (track->effect)
            track->effect->releaseResources();
}

void MainComponent::paint(juce::Graphics& graphics)
{
    graphics.fillAll(backgroundColour);
    graphics.setColour(panelColour);
    graphics.fillRect(0, 0, getWidth(), 64);
    graphics.fillRect(0, 65, 256, getHeight() - 65);
    graphics.fillRect(getWidth() - 263, 65, 263, getHeight() - 65);

    graphics.setColour(accentColour);
    graphics.fillRoundedRectangle(20.0f, 20.0f, 14.0f, 14.0f, 4.0f);
    graphics.setColour(mutedTextColour);
    graphics.setFont(juce::Font(9.0f, juce::Font::bold));
    graphics.drawText("MULTITRACK AUDIO WORKSPACE", 42, 37, 220, 14,
                      juce::Justification::centredLeft);

    graphics.setColour(borderColour);
    graphics.drawHorizontalLine(64, 0.0f, static_cast<float>(getWidth()));
    graphics.drawVerticalLine(256, 65.0f, static_cast<float>(getHeight()));
    graphics.drawVerticalLine(getWidth() - 264, 65.0f, static_cast<float>(getHeight()));
    graphics.setColour(borderColour.withAlpha(0.65f));
    graphics.drawHorizontalLine(112, 16.0f, 240.0f);
    graphics.drawHorizontalLine(112, static_cast<float>(getWidth() - 248),
                                static_cast<float>(getWidth() - 16));
}

void MainComponent::resized()
{
    const auto width = getWidth();
    const auto height = getHeight();
    titleLabel.setBounds(38, 5, 220, 30);
    playButton.setBounds(280, 14, 76, 36);
    stopButton.setBounds(364, 14, 76, 36);
    timeLabel.setBounds(454, 14, 130, 36);

    const auto panelHeight = height - 65;
    libraryLabel.setBounds(20, 86, 216, 22);
    importButton.setBounds(16, 118, 224, 36);
    sampleList.setBounds(16, 166, 224, juce::jmax(100, panelHeight - 250));
    addSampleButton.setBounds(16, height - 72, 224, 40);

    const auto centerWidth = juce::jmax(320, width - 520);
    arrangement.setBounds(256, 65, centerWidth, panelHeight);

    const auto rightX = width - 248;
    inspectorLabel.setBounds(rightX, 86, 216, 22);
    trackNameLabel.setBounds(rightX, 122, 216, 34);
    gainValueLabel.setBounds(rightX, 176, 216, 24);
    gainSlider.setBounds(rightX, 205, 216, 30);
    muteButton.setBounds(rightX, 250, 216, 30);
    effectLabel.setBounds(rightX, 318, 216, 22);
    loadPluginButton.setBounds(rightX, 350, 216, 36);
}

void MainComponent::timerCallback()
{
    if (playing.load())
    {
        const juce::ScopedLock lock(audioLock);
        if (!tracks.empty())
        {
            playheadSeconds = tracks.front()->transport.getCurrentPosition();
            const auto totalMilliseconds = static_cast<int>(playheadSeconds * 1000.0);
            const auto minutes = totalMilliseconds / 60000;
            const auto seconds = (totalMilliseconds / 1000) % 60;
            const auto milliseconds = totalMilliseconds % 1000;
            timeLabel.setText(juce::String::formatted("%02d:%02d.%03d", minutes, seconds, milliseconds),
                              juce::dontSendNotification);
            if (playheadSeconds >= tracks.front()->durationSeconds)
            {
                playing = false;
                for (auto& track : tracks)
                    track->transport.stop();
            }
        }
    }
    arrangement.repaint();
}

void MainComponent::importLibraryFolder()
{
    juce::FileChooser chooser("Import audio into the sample library", juce::File(), "");
    if (!chooser.browseForDirectory())
        return;

    const auto destination = getLibraryDirectory();
    destination.createDirectory();
    juce::Array<juce::File> found;
    chooser.getResult().findChildFiles(found, juce::File::findFiles, true);
    int imported = 0;
    for (const auto& file : found)
    {
        if (!isSupportedAudioFile(file))
            continue;
        const auto target = destination.getNonexistentChildFile(file.getFileNameWithoutExtension(),
                                                               file.getFileExtension(), false);
        if (file.copyFileTo(target))
            ++imported;
    }

    refreshLibrary();
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Sample library",
        juce::String(imported) + " audio file(s) imported to " + destination.getFullPathName());
}

void MainComponent::addSampleToArrangement(const juce::File& file)
{
    auto* reader = formatManager.createReaderFor(file);
    if (reader == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Unsupported audio",
                                             "This file could not be opened by the available audio formats.");
        return;
    }

    auto track = std::make_unique<Track>();
    track->name = file.getFileNameWithoutExtension();
    track->file = file;
    track->durationSeconds = static_cast<double>(reader->lengthInSamples) / reader->sampleRate;
    track->waveform = makeWaveform(*reader);
    const auto sampleRate = reader->sampleRate;
    track->readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
    track->transport.setSource(track->readerSource.get(), 32768, nullptr, sampleRate);
    track->scratch.setSize(2, 512);

    {
        const juce::ScopedLock lock(audioLock);
        tracks.push_back(std::move(track));
        selectedTrack = static_cast<int>(tracks.size()) - 1;
        if (getAudioDeviceManager().getCurrentAudioDevice() != nullptr)
        {
            auto& added = *tracks.back();
            if (added.effect)
                added.effect->prepareToPlay(getSampleRate(), getBlockSize());
        }
    }

    selectTrack(selectedTrack);
    arrangement.repaint();
}

void MainComponent::refreshLibrary()
{
    const auto folder = getLibraryDirectory();
    folder.createDirectory();
    sampleModel.refresh();
    sampleList.updateContent();
    sampleList.repaint();
}

void MainComponent::loadVst3()
{
    if (selectedTrack < 0 || selectedTrack >= static_cast<int>(tracks.size()))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Load VST3",
                                             "Select a track before loading an effect.");
        return;
    }

    juce::FileChooser chooser("Select a VST3 plugin bundle", juce::File(), "");
    if (!chooser.browseForDirectory())
        return;

    juce::AudioPluginFormat* vst3Format = nullptr;
    for (int index = 0; index < pluginFormats.getNumFormats(); ++index)
    {
        auto* format = pluginFormats.getFormat(index);
        if (format->getName().containsIgnoreCase("VST3"))
        {
            vst3Format = format;
            break;
        }
    }

    if (vst3Format == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "VST3 unavailable",
                                             "This build does not include a VST3 host format.");
        return;
    }

    juce::OwnedArray<juce::PluginDescription> descriptions;
    vst3Format->findAllTypesForFile(descriptions, chooser.getResult().getFullPathName());
    if (descriptions.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Plugin not found",
                                             "No loadable VST3 plugin was found in that directory.");
        return;
    }

    juce::String error;
    auto instance = pluginFormats.createPluginInstance(*descriptions.getFirst(), getSampleRate(), getBlockSize(), error);
    if (instance == nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Plugin load failed", error);
        return;
    }

    instance->setPlayConfigDetails(2, 2, getSampleRate(), getBlockSize());
    instance->prepareToPlay(getSampleRate(), getBlockSize());
    const auto pluginName = instance->getName();
    {
        const juce::ScopedLock lock(audioLock);
        tracks[static_cast<size_t>(selectedTrack)]->effect = std::move(instance);
    }
    effectLabel.setText("EFFECTS  /  " + pluginName, juce::dontSendNotification);
}

void MainComponent::selectTrack(int index)
{
    if (index < 0 || index >= static_cast<int>(tracks.size()))
        return;

    selectedTrack = index;
    const auto& track = *tracks[static_cast<size_t>(index)];
    trackNameLabel.setText(track.name, juce::dontSendNotification);
    gainSlider.setValue(track.gain.load(), juce::dontSendNotification);
    muteButton.setToggleState(track.muted.load(), juce::dontSendNotification);
    const auto decibels = juce::Decibels::gainToDecibels(track.gain.load(), -100.0);
    gainValueLabel.setText("LEVEL     " + juce::String(decibels, 1) + " dB", juce::dontSendNotification);
    effectLabel.setText(track.effect ? "EFFECTS  /  " + track.effect->getName() : "EFFECTS",
                        juce::dontSendNotification);
    arrangement.repaint();
}

juce::File MainComponent::getLibraryDirectory() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Dawsoftware").getChildFile("Samples");
}

bool MainComponent::isSupportedAudioFile(const juce::File& file)
{
    const auto extension = file.getFileExtension().toLowerCase();
    return extension == ".wav" || extension == ".aif" || extension == ".aiff"
        || extension == ".flac" || extension == ".ogg" || extension == ".mp3";
}

std::vector<float> MainComponent::makeWaveform(juce::AudioFormatReader& reader)
{
    constexpr int pointCount = 180;
    std::vector<float> waveform(static_cast<size_t>(pointCount), 0.0f);
    juce::AudioBuffer<float> chunk(1, 2048);
    const auto length = reader.lengthInSamples;
    if (length <= 0)
        return waveform;

    for (int point = 0; point < pointCount; ++point)
    {
        const auto start = static_cast<juce::int64>(point) * length / pointCount;
        const auto end = static_cast<juce::int64>(point + 1) * length / pointCount;
        const auto count = static_cast<int>(juce::jmin<juce::int64>(chunk.getNumSamples(), end - start));
        if (count <= 0)
            continue;
        chunk.clear();
        if (reader.read(&chunk, 0, count, start, true, true))
            waveform[static_cast<size_t>(point)] = chunk.getMagnitude(0, 0, count);
    }
    return waveform;
}

int MainComponent::SampleListModel::getNumRows()
{
    return files.size();
}

void MainComponent::SampleListModel::paintListBoxItem(int row, juce::Graphics& graphics, int width,
                                                       int height, bool selected)
{
    if (!juce::isPositiveAndBelow(row, files.size()))
        return;
    graphics.fillAll(selected ? juce::Colour(0xff30383a) : panelColour);
    graphics.setColour(selected ? accentColour : textColour);
    graphics.setFont(juce::Font(12.0f));
    graphics.drawText(files.getReference(row).getFileNameWithoutExtension(), 12, 0, width - 20, height,
                      juce::Justification::centredLeft, true);
}

void MainComponent::SampleListModel::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    const auto file = fileForRow(row);
    if (file.existsAsFile())
        main.addSampleToArrangement(file);
}

juce::File MainComponent::SampleListModel::fileForRow(int row) const
{
    return juce::isPositiveAndBelow(row, files.size()) ? files.getReference(row) : juce::File();
}

void MainComponent::SampleListModel::refresh()
{
    files.clear();
    main.getLibraryDirectory().findChildFiles(files, juce::File::findFiles, false);
    for (int index = files.size(); --index >= 0;)
        if (!MainComponent::isSupportedAudioFile(files.getReference(index)))
            files.remove(index);
    files.sort();
}

void MainComponent::ArrangementView::paint(juce::Graphics& graphics)
{
    graphics.fillAll(backgroundColour);
    constexpr int headerHeight = 38;
    constexpr int labelWidth = 164;
    graphics.setColour(panelColour);
    graphics.fillRect(0, 0, getWidth(), headerHeight);
    graphics.setColour(borderColour);
    graphics.drawHorizontalLine(headerHeight, 0.0f, static_cast<float>(getWidth()));

    constexpr int secondsVisible = 16;
    const auto timelineWidth = juce::jmax(1, getWidth() - labelWidth);
    graphics.setColour(mutedTextColour);
    graphics.setFont(juce::Font(9.0f, juce::Font::bold));
    graphics.drawText("TRACKS", 14, 0, labelWidth - 24, headerHeight,
                      juce::Justification::centredLeft);

    for (int second = 0; second <= secondsVisible; ++second)
    {
        const auto x = labelWidth + second * timelineWidth / secondsVisible;
        graphics.setColour(borderColour.withAlpha(second % 4 == 0 ? 0.8f : 0.38f));
        graphics.drawVerticalLine(x, static_cast<float>(headerHeight), static_cast<float>(getHeight()));
        if (second < secondsVisible)
        {
            for (int subdivision = 1; subdivision < 4; ++subdivision)
            {
                const auto tickX = x + subdivision * timelineWidth / (secondsVisible * 4);
                graphics.setColour(borderColour.withAlpha(0.2f));
                graphics.drawVerticalLine(tickX, static_cast<float>(headerHeight),
                                          static_cast<float>(getHeight()));
            }
        }
        if (second < secondsVisible)
        {
            graphics.setColour(mutedTextColour);
            graphics.setFont(juce::Font(10.0f));
            graphics.drawText(juce::String(second) + "s", x + 5, 0, 34, headerHeight,
                              juce::Justification::centredLeft);
        }
    }

    const juce::ScopedLock lock(main.audioLock);
    if (main.tracks.empty())
    {
        const auto messageBounds = juce::Rectangle<int>(labelWidth + 24, headerHeight + 44,
                                                         juce::jmax(0, timelineWidth - 48),
                                                         juce::jmax(0, getHeight() - headerHeight - 88));
        if (messageBounds.getHeight() >= 72)
        {
            graphics.setColour(textColour);
            graphics.setFont(juce::Font(17.0f, juce::Font::bold));
            graphics.drawText("Your arrangement starts here", messageBounds.removeFromTop(34),
                              juce::Justification::centred);
            graphics.setColour(mutedTextColour);
            graphics.setFont(juce::Font(12.0f));
            graphics.drawText("Add a sample from the library or drop an audio file into this area.",
                              messageBounds, juce::Justification::centred, true);
        }
    }

    for (size_t index = 0; index < main.tracks.size(); ++index)
    {
        const auto y = headerHeight + static_cast<int>(index) * trackRowHeight;
        if (y >= getHeight())
            break;
        const auto& track = *main.tracks[index];
        if (static_cast<int>(index) == main.selectedTrack)
        {
            graphics.setColour(juce::Colour(0xff22292a));
            graphics.fillRect(0, y, getWidth(), trackRowHeight);
            graphics.setColour(accentColour);
            graphics.fillRect(0, y + 1, 3, trackRowHeight - 2);
        }

        graphics.setColour(borderColour.withAlpha(0.65f));
        graphics.drawHorizontalLine(y + trackRowHeight - 1, 0.0f, static_cast<float>(getWidth()));
        const auto trackMuted = track.muted.load();
        graphics.setColour(trackMuted ? mutedTextColour : textColour);
        graphics.setFont(juce::Font(12.0f, juce::Font::bold));
        graphics.drawFittedText(track.name, 14, y + 9, labelWidth - 40, 20,
                                juce::Justification::centredLeft, 1);
        graphics.setColour(mutedTextColour);
        graphics.setFont(juce::Font(10.0f));
        graphics.drawText(trackMuted ? "MUTED" : (track.effect ? track.effect->getName() : "AUDIO"),
                          14, y + 33, labelWidth - 24, 18,
                          juce::Justification::centredLeft, true);
        graphics.setColour(mutedTextColour.withAlpha(0.7f));
        graphics.setFont(juce::Font(9.0f, juce::Font::bold));
        graphics.drawText(juce::String(static_cast<int>(index) + 1).paddedLeft('0', 2),
                          labelWidth - 28, y + 10, 18, 16, juce::Justification::centredRight);

        const auto clipWidth = juce::jlimit(24, timelineWidth,
            static_cast<int>(track.durationSeconds / secondsVisible * timelineWidth));
        const auto clip = juce::Rectangle<int>(labelWidth + 3, y + 11, clipWidth - 6, trackRowHeight - 22);
        graphics.setColour(trackMuted ? juce::Colour(0xff303638)
                           : static_cast<int>(index) == main.selectedTrack
                               ? juce::Colour(0xff53632f) : juce::Colour(0xff2e4b4d));
        graphics.fillRoundedRectangle(clip.toFloat(), 4.0f);
        graphics.setColour(trackMuted ? mutedTextColour.withAlpha(0.55f)
                           : static_cast<int>(index) == main.selectedTrack
                               ? accentColour : juce::Colour(0xff83c7bd));
        const auto& waveform = track.waveform;
        if (!waveform.empty())
        {
            const auto step = juce::jmax(1, static_cast<int>(waveform.size() * 2 / juce::jmax(1, clip.getWidth())));
            for (int point = 0, x = clip.getX() + 5; point < static_cast<int>(waveform.size()) && x < clip.getRight() - 3;
                 point += step, x += 2)
            {
                const auto amplitude = juce::jmax(2.0f, waveform[static_cast<size_t>(point)] * (clip.getHeight() - 12) * 0.5f);
                graphics.drawVerticalLine(x, clip.getCentreY() - amplitude, clip.getCentreY() + amplitude);
            }
        }
    }

    if (main.playheadSeconds >= 0.0)
    {
        const auto x = labelWidth + static_cast<int>(main.playheadSeconds / secondsVisible * timelineWidth);
        if (x < getWidth())
        {
            graphics.setColour(accentColour);
            graphics.drawVerticalLine(x, static_cast<float>(headerHeight), static_cast<float>(getHeight()));
        }
    }
}

void MainComponent::ArrangementView::mouseDown(const juce::MouseEvent& event)
{
    if (event.x < 164 || event.y < 38)
        return;
    const auto index = (event.y - 38) / trackRowHeight;
    main.selectTrack(index);
}

bool MainComponent::ArrangementView::isInterestedInFileDrag(const juce::StringArray& files)
{
    return files.size() > 0;
}

void MainComponent::ArrangementView::filesDropped(const juce::StringArray& files, int, int)
{
    for (const auto& path : files)
    {
        const juce::File file(path);
        if (MainComponent::isSupportedAudioFile(file))
            main.addSampleToArrangement(file);
    }
}