// sk4n_ui_snapshot: render the real plugin editor headlessly to a PNG.
//
//   sk4n_ui_snapshot <out.png> [scale] [width height]   (scale defaults to 2.0; width/height
//                                                         default to the editor's own default
//                                                         size, kEditorW x getCompactHeight() --
//                                                         pass (kEditorW-100) (getCompactHeight()-60)
//                                                         to check the minimum resize floor)
//
// The look-and-feel regression gate for the ZQ SFX house UI style (zqsfx_ui module):
// render before a
// UI change, render after, compare. Mirrors Project_Lfl0w's lflow_ui_snapshot (which itself
// mirrors the sibling Broken plugin's ui snapshot tool), but SK4n never calls juce_generate_juce_header
// and never includes a generated JuceHeader.h -- every SK4n source includes JUCE module headers
// directly -- so the JUCE_GENERATED_SOURCES_DIRECTORY propagation trick that lflow_ui_snapshot
// needed does not apply here; this tool links straight against the plugin's own shared-code
// CMake target ("SK4n", produced by juce_add_plugin) and includes PluginProcessor.h /
// PluginEditor.h exactly like any other translation unit under Source/.
//
// Determinism: nothing here ever pumps JUCE's message loop (no runDispatchLoop), so none of the
// editor's own juce::Timer callbacks (BufferDisplay, CircularMorpher, meters, KnobControl, etc.,
// all started in their constructors) ever actually fire before the snapshot is taken -- JUCE
// dispatches timer callbacks through the message queue, not directly from a background timer
// thread. The snapshot is taken immediately after construction, before any timer tick, which is
// what makes two successive renders of unchanged code byte-identical (see
// docs/ui_migration_report.md for the render-twice-and-cmp check this was verified with).

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>

int main (int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: sk4n_ui_snapshot <out.png> [scale] [width height] [disclosure]"
                     " [param=value ...]\n"
                     "       disclosure: none|oscillators|filter|fx|modulation|advanced\n"
                     "       param=value sets an APVTS parameter before the editor is built,\n"
                     "       e.g. filterMode=1 to render the Cabinet row instead of 8-Pole.\n";
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (juce::String (argv[1]));
    const float scale = argc > 2 ? juce::String (argv[2]).getFloatValue() : 2.0f;

    // Processor declared before editor: C++ destroys locals in reverse declaration order, so
    // the editor is always torn down before the processor it references (spec requirement --
    // "delete the editor before the processor").
    SK4nAudioProcessor processor;

    // Any trailing name=value argument sets a parameter BEFORE the editor is built, so the gate
    // can render a non-default state. Without this it could only ever see each mode switch at its
    // default index -- the blind spot that let the Filter/FX overlapping-rows bug survive.
    for (int i = 2; i < argc; ++i)
    {
        const juce::String arg (argv[i]);
        if (! arg.containsChar ('=')) continue;

        const auto id  = arg.upToFirstOccurrenceOf ("=", false, false).trim();
        const auto val = arg.fromFirstOccurrenceOf ("=", false, false).trim().getFloatValue();

        if (auto* p = processor.apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (val));
        else
            std::cerr << "warning: no such parameter '" << id << "'\n";
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    if (editor == nullptr)
    {
        std::cerr << "createEditor returned null\n";
        return 1;
    }

    // Optional: expand one disclosure before rendering. Without this every section panel is
    // collapsed and the gate can only ever see the compact view, which is how the Filter/FX
    // overlapping-knob-rows bug went unnoticed through the entire house-UI migration.
    if (argc > 5)
    {
        using D = SK4nAudioProcessorEditor::Disclosure;
        const auto name = juce::String (argv[5]).toLowerCase();
        const D d = name == "oscillators" ? D::Oscillators
                  : name == "filter"      ? D::Filter
                  : name == "fx"          ? D::FX
                  : name == "modulation"  ? D::Modulation
                  : name == "advanced"    ? D::Advanced
                                          : D::None;

        if (d == D::None && name != "none")
        {
            std::cerr << "unknown disclosure '" << name << "'\n";
            return 2;
        }

        if (auto* sk = dynamic_cast<SK4nAudioProcessorEditor*> (editor.get()))
            sk->setDisclosure (d);   // resizes the editor to fit the expanded content
    }

    // After the disclosure, so an explicit size still wins. Non-positive values mean "not given"
    // (a caller passing "" for width/height would otherwise collapse the editor to nothing).
    if (argc > 4)
    {
        const int w = juce::String (argv[3]).getIntValue();
        const int h = juce::String (argv[4]).getIntValue();
        if (w > 0 && h > 0)
            editor->setSize (w, h); // clamped to the editor's own setResizeLimits() if out of range
    }

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, scale);

    out.getParentDirectory().createDirectory();
    out.deleteFile();
    juce::FileOutputStream stream (out);
    juce::PNGImageFormat png;
    if (! stream.openedOk() || ! png.writeImageToStream (image, stream))
    {
        std::cerr << "could not write " << out.getFullPathName() << "\n";
        return 1;
    }

    std::cout << out.getFullPathName() << "  " << image.getWidth() << "x" << image.getHeight() << "\n";
    return 0;
}
