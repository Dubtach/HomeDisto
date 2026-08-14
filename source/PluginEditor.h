#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class MinimalistSynthLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MinimalistSynthLookAndFeel()
    {
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::white); 
        setColour(juce::Slider::trackColourId, juce::Colour(0xFF000000).withAlpha(0.5f));
        
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF161618));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF2A2A30));
        setColour(juce::ComboBox::textColourId, juce::Colours::white);
        setColour(juce::ComboBox::arrowColourId, juce::Colour(0xFFFFFFFF));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, 
                           float sliderPos, const float rotaryStartAngle, 
                           const float rotaryEndAngle, juce::Slider& slider) override
    {
        auto radius = (float) juce::jmin (width / 2, height / 2) - 4.0f;
        auto centreX = (float) x + (float) width  * 0.5f;
        auto centreY = (float) y + (float) height * 0.5f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        auto neonColour = slider.findColour(juce::Slider::rotarySliderFillColourId);

        g.setColour(juce::Colour(0xFF0A0A0C));
        g.fillEllipse(centreX - radius + 2.0f, centreY - radius + 2.0f, (radius - 2.0f) * 2.0f, (radius - 2.0f) * 2.0f);

        // NEW: small tick marks around the knob's travel -- a detail real
        // analog-style knobs almost always have, and one this one was
        // missing entirely.
        {
            const int numTicks = 11;
            g.setColour(juce::Colours::white.withAlpha(0.18f));
            for (int t = 0; t < numTicks; ++t)
            {
                float tickAngle = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * ((float) t / (float) (numTicks - 1));
                float inner = radius + 2.0f;
                float outer = radius + 5.0f;
                juce::Point<float> p1(centreX + inner * std::sin(tickAngle), centreY - inner * std::cos(tickAngle));
                juce::Point<float> p2(centreX + outer * std::sin(tickAngle), centreY - outer * std::cos(tickAngle));
                g.drawLine({p1, p2}, (t == 0 || t == numTicks - 1 || t == numTicks / 2) ? 1.4f : 1.0f);
            }
        }

        juce::Path bgArc;
        bgArc.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xFF000000).withAlpha(0.4f)); 
        g.strokePath(bgArc, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path fillArc;
        fillArc.addCentredArc(centreX, centreY, radius, radius, 0.0f, rotaryStartAngle, angle, true);
        
        g.setColour(neonColour.withAlpha(0.6f));
        g.strokePath(fillArc, juce::PathStrokeType(14.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        
        g.setColour(juce::Colours::white);
        g.strokePath(fillArc, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour(juce::Colours::white);
        g.fillEllipse(centreX - 3.5f, centreY - 3.5f, 7.0f, 7.0f);

        juce::Path pointer;
        pointer.startNewSubPath(centreX, centreY);
        pointer.lineTo(centreX + (radius - 7.0f) * std::sin(angle), centreY - (radius - 7.0f) * std::cos(angle));
        
        g.setColour(neonColour.withAlpha(0.5f));
        g.strokePath(pointer, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(juce::Colours::white);
        g.strokePath(pointer, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawEllipse(centreX - (radius - 7.0f), centreY - (radius - 7.0f), (radius - 7.0f) * 2.0f, (radius - 7.0f) * 2.0f, 1.0f);
    }

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, 
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto fontSize = 14.0f;
        auto tickWidth = 16.0f;
        
        juce::Rectangle<float> tickBounds (0.0f, ((float) button.getHeight() - tickWidth) * 0.5f, tickWidth, tickWidth);
        
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.drawRoundedRectangle(tickBounds.translated(0.0f, 1.5f), 3.0f, 2.5f);
        
        g.setColour(juce::Colour(0xFF09090B)); 
        g.drawRoundedRectangle(tickBounds, 3.0f, 2.5f);
        
        if (button.getToggleState())
        {
            auto tickColour = button.findColour(juce::ToggleButton::tickColourId);
            juce::Path tickPath;
            tickPath.startNewSubPath(tickBounds.getX() + 3.0f, tickBounds.getCentreY());
            tickPath.lineTo(tickBounds.getCentreX() - 1.0f, tickBounds.getBottom() - 4.0f);
            tickPath.lineTo(tickBounds.getRight() - 2.0f, tickBounds.getY() + 2.0f);
            
            g.setColour(tickColour.withAlpha(0.6f));
            g.strokePath(tickPath, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            
            g.setColour(juce::Colours::white);
            g.strokePath(tickPath, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        
        g.setFont(juce::FontOptions(fontSize).withName("Helvetica").withStyle("Bold"));
        auto textBounds = button.getLocalBounds().toFloat().withTrimmedLeft(tickWidth + 6.0f);
        
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.drawText(button.getButtonText(), textBounds.translated(0.0f, 1.5f), juce::Justification::centredLeft);
        
        g.setColour(juce::Colour(0xFF09090B));
        g.drawText(button.getButtonText(), textBounds, juce::Justification::centredLeft);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();

        if (button.getName() == "LOCK")
        {
            // FIX: locked vs unlocked was only a subtle icon-colour change,
            // which was genuinely hard to tell apart at a glance ("hard to
            // notice even if it's off"). Locked now fills the whole circle
            // solid, with a dark icon on top -- an unmistakable "this is
            // pressed/active" look, the same visual language a toggled
            // mode/bypass button already uses elsewhere in this UI.
            // Unlocked is now a plain, mostly-empty outline circle.
            bool locked = button.getToggleState();
            auto cX = bounds.getCentreX();
            auto cY = bounds.getCentreY() + 1.0f;

            if (locked)
            {
                g.setColour(juce::Colour(0xFF00E5FF).withAlpha(shouldDrawButtonAsHighlighted ? 1.0f : 0.9f));
                g.fillEllipse(bounds.reduced(1.0f));
                g.setColour(juce::Colour(0xFF09090B));
            }
            else
            {
                g.setColour(juce::Colours::black.withAlpha(shouldDrawButtonAsHighlighted ? 0.4f : 0.25f));
                g.fillEllipse(bounds.reduced(1.0f));
                g.setColour(juce::Colours::white.withAlpha(0.4f));
                g.drawEllipse(bounds.reduced(1.0f), 1.0f);
                g.setColour(juce::Colours::white.withAlpha(shouldDrawButtonAsHighlighted ? 0.8f : 0.5f));
            }

            // shackle
            juce::Path shackle;
            shackle.addCentredArc(cX, cY - 3.5f, 3.0f, 3.0f, 0.0f,
                                   juce::MathConstants<float>::pi * 1.5f,
                                   juce::MathConstants<float>::pi * 2.5f, true);
            g.strokePath(shackle, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // body
            g.fillRoundedRectangle(cX - 3.5f, cY - 1.0f, 7.0f, 5.5f, 1.0f);
            return;
        }

        // NEW: same lock icon as above, but tinted to match the EQ card's
        // own green accent instead of the cyan used elsewhere -- kept as a
        // separate name rather than changing "LOCK" itself, so the
        // existing OUTPUT/MIX locks are untouched.
        if (button.getName() == "LOCK_EQ")
        {
            bool locked = button.getToggleState();
            auto cX = bounds.getCentreX();
            auto cY = bounds.getCentreY() + 1.0f;

            if (locked)
            {
                g.setColour(juce::Colour(0xFF00E5FF).withAlpha(shouldDrawButtonAsHighlighted ? 1.0f : 0.9f));
                g.fillEllipse(bounds.reduced(1.0f));
                g.setColour(juce::Colour(0xFF09090B));
            }
            else
            {
                g.setColour(juce::Colours::black.withAlpha(shouldDrawButtonAsHighlighted ? 0.4f : 0.25f));
                g.fillEllipse(bounds.reduced(1.0f));
                g.setColour(juce::Colours::white.withAlpha(0.4f));
                g.drawEllipse(bounds.reduced(1.0f), 1.0f);
                g.setColour(juce::Colours::white.withAlpha(shouldDrawButtonAsHighlighted ? 0.8f : 0.5f));
            }

            juce::Path shackle;
            shackle.addCentredArc(cX, cY - 3.5f, 3.0f, 3.0f, 0.0f,
                                   juce::MathConstants<float>::pi * 1.5f,
                                   juce::MathConstants<float>::pi * 2.5f, true);
            g.strokePath(shackle, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.fillRoundedRectangle(cX - 3.5f, cY - 1.0f, 7.0f, 5.5f, 1.0f);
            return;
        }

        // NEW: reset icon (circular arrow) for the EQ's "flatten to
        // neutral" button. FIX: outline/backdrop given more contrast to
        // match the lock icons' visibility fix above.
        if (button.getName() == "RESET_EQ")
        {
            auto cX = bounds.getCentreX();
            auto cY = bounds.getCentreY();

            g.setColour(juce::Colours::black.withAlpha(shouldDrawButtonAsHighlighted ? 0.4f : 0.25f));
            g.fillEllipse(bounds.reduced(1.0f));
            g.setColour(juce::Colours::white.withAlpha(0.35f));
            g.drawEllipse(bounds.reduced(1.0f), 1.0f);

            g.setColour(juce::Colours::white.withAlpha(shouldDrawButtonAsHighlighted ? 0.95f : 0.7f));
            juce::Path arc;
            float r = 4.5f;
            arc.addCentredArc(cX, cY, r, r, 0.0f,
                               juce::MathConstants<float>::pi * 0.25f,
                               juce::MathConstants<float>::pi * 1.85f, true);
            g.strokePath(arc, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // arrowhead at the arc's leading end
            float headAngle = juce::MathConstants<float>::pi * 0.25f;
            juce::Point<float> tip(cX + r * std::sin(headAngle), cY - r * std::cos(headAngle));
            juce::Path head;
            head.startNewSubPath(tip.x - 3.0f, tip.y - 1.0f);
            head.lineTo(tip.x + 1.5f, tip.y - 2.5f);
            head.lineTo(tip.x + 1.0f, tip.y + 2.0f);
            head.closeSubPath();
            g.fillPath(head);
            return;
        }

        if (button.getName() == "SAVE" || button.getName() == "SETTINGS" || button.getName() == "BYPASS")
        {
            g.setColour(shouldDrawButtonAsHighlighted ? juce::Colour(0xFF2A2A30) : juce::Colour(0xFF161618));
            g.fillRoundedRectangle(bounds, 4.0f);
            
            g.setColour(juce::Colours::white);
            if (button.getName() == "SAVE")
            {
                g.drawRoundedRectangle(bounds.reduced(6.0f), 2.0f, 1.5f);
                g.fillRect(bounds.getX() + 10, bounds.getY() + 6, bounds.getWidth() - 20, 6.0f);
                g.fillRect(bounds.getX() + 8, bounds.getBottom() - 12, bounds.getWidth() - 16, 6.0f);
            }
            else if (button.getName() == "SETTINGS")
            {
                auto cX = bounds.getCentreX();
                auto cY = bounds.getCentreY();
                for (int i = 0; i < 6; ++i)
                {
                    juce::Path spoke;
                    spoke.addRectangle(-1.5f, -9.0f, 3.0f, 18.0f);
                    spoke.applyTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::pi * i / 3.0f).translated(cX, cY));
                    g.fillPath(spoke);
                }
                g.setColour(shouldDrawButtonAsHighlighted ? juce::Colour(0xFF2A2A30) : juce::Colour(0xFF161618));
                g.fillEllipse(cX - 5, cY - 5, 10, 10);
                g.setColour(juce::Colours::white);
                g.drawEllipse(cX - 5, cY - 5, 10, 10, 1.5f);
            }
            else if (button.getName() == "BYPASS")
            {
                auto cX = bounds.getCentreX();
                auto cY = bounds.getCentreY();
                
                juce::Path powerArc;
                float gap = 0.5f; 
                powerArc.addCentredArc(cX, cY, 6.0f, 6.0f, 0.0f, gap, juce::MathConstants<float>::twoPi - gap, true);
                g.strokePath(powerArc, juce::PathStrokeType(1.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
                g.drawLine(cX, cY - 6.0f, cX, cY + 1.0f, 1.5f);
            }
            return;
        }

        if (button.getToggleState())
        {
            g.setColour(juce::Colours::black.withAlpha(0.4f));
            g.fillRoundedRectangle(bounds.translated(0.0f, 2.0f), 4.0f);
            
            g.setColour(juce::Colour(0xFF09090B)); 
            g.fillRoundedRectangle(bounds, 4.0f);
            
            g.setColour(juce::Colours::white);
            g.fillRoundedRectangle(0, 0, 6.0f, bounds.getHeight(), 2.0f);
            
            g.setColour(juce::Colours::white.withAlpha(0.15f));
            g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            g.setColour(juce::Colour(0xFF1E1E24).withAlpha(0.8f));
            g.fillRoundedRectangle(bounds, 4.0f);
            g.setColour(juce::Colours::black.withAlpha(0.2f));
            g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        }
        else
        {
            g.setColour(juce::Colour(0xFF121215).withAlpha(0.6f));
            g.fillRoundedRectangle(bounds, 4.0f);
            g.setColour(juce::Colours::black.withAlpha(0.15f));
            g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
        }
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        if (button.getName() == "SAVE" || button.getName() == "SETTINGS" || button.getName() == "BYPASS" || button.getName() == "LOCK" || button.getName() == "LOCK_EQ" || button.getName() == "RESET_EQ") return;

        // NEW: each distortion mode gets its own small icon so the six
        // options are recognisable at a glance, not just six identical
        // buttons differentiated only by a word.
        if (button.getName() == "MODE_BTN")
        {
            auto bounds = button.getLocalBounds().toFloat();
            juce::Colour col = button.getToggleState() ? juce::Colours::white : juce::Colours::white.withAlpha(0.55f);
            juce::String mode = button.getButtonText();

            // FIX: icon was pinned to the left edge with text trailing
            // after it, leaving the button's right side looking
            // unbalanced. Now the icon+divider+text group is centred as a
            // single unit within the button.
            juce::Font labelFont(juce::FontOptions(11.0f).withName("Helvetica").withStyle(button.getToggleState() ? "Bold" : "Plain"));
            float textWidth = (float) juce::GlyphArrangement::getStringWidthInt(labelFont, mode);
            const float iconWidth = 16.0f, dividerGap = 9.0f;
            float groupWidth = iconWidth + dividerGap + textWidth;
            float groupLeft = bounds.getCentreX() - groupWidth * 0.5f;

            float iconCx = groupLeft + iconWidth * 0.5f;
            float iconCy = bounds.getCentreY();
            g.setColour(col);

            if (mode == "PUNCH")
            {
                // 4-point spark/impact star
                juce::Path star;
                star.startNewSubPath(iconCx, iconCy - 7.0f);
                star.lineTo(iconCx + 2.0f, iconCy - 2.0f);
                star.lineTo(iconCx + 7.0f, iconCy);
                star.lineTo(iconCx + 2.0f, iconCy + 2.0f);
                star.lineTo(iconCx, iconCy + 7.0f);
                star.lineTo(iconCx - 2.0f, iconCy + 2.0f);
                star.lineTo(iconCx - 7.0f, iconCy);
                star.lineTo(iconCx - 2.0f, iconCy - 2.0f);
                star.closeSubPath();
                g.fillPath(star);
            }
            else if (mode == "TUBE")
            {
                // valve/bulb silhouette with two base pins -- simplified,
                // no internal clutter, reads clean at this size.
                juce::Rectangle<float> bulb(iconCx - 3.5f, iconCy - 7.0f, 7.0f, 10.5f);
                g.drawRoundedRectangle(bulb, 3.5f, 1.4f);
                g.drawLine(iconCx - 1.8f, iconCy + 3.5f, iconCx - 1.8f, iconCy + 7.0f, 1.4f);
                g.drawLine(iconCx + 1.8f, iconCy + 3.5f, iconCx + 1.8f, iconCy + 7.0f, 1.4f);
            }
            else if (mode == "TAPE")
            {
                // single reel: rim + hub + three spokes
                g.drawEllipse(iconCx - 6.0f, iconCy - 6.0f, 12.0f, 12.0f, 1.4f);
                g.fillEllipse(iconCx - 1.6f, iconCy - 1.6f, 3.2f, 3.2f);
                for (int a = 0; a < 3; ++a)
                {
                    float ang = a * juce::MathConstants<float>::twoPi / 3.0f + juce::MathConstants<float>::halfPi;
                    g.drawLine(iconCx, iconCy, iconCx + std::cos(ang) * 5.2f, iconCy + std::sin(ang) * 5.2f, 1.3f);
                }
            }
            else if (mode == "DIGITAL")
            {
                // crisp 2x2 pixel grid, rounded to match the plugin's
                // rounded-rect language elsewhere
                float s = 3.4f, gap = 1.6f;
                for (int gx = 0; gx < 2; ++gx)
                    for (int gy = 0; gy < 2; ++gy)
                        g.fillRoundedRectangle(iconCx - s - gap * 0.5f + gx * (s + gap),
                                                iconCy - s - gap * 0.5f + gy * (s + gap), s, s, 0.8f);
            }
            else if (mode == "CRUNCH")
            {
                // single clean lightning bolt, filled -- reads instantly
                // as "aggressive" at a glance
                juce::Path bolt;
                bolt.startNewSubPath(iconCx + 1.5f, iconCy - 7.5f);
                bolt.lineTo(iconCx - 4.5f, iconCy + 1.0f);
                bolt.lineTo(iconCx - 0.5f, iconCy + 1.0f);
                bolt.lineTo(iconCx - 1.5f, iconCy + 7.5f);
                bolt.lineTo(iconCx + 4.5f, iconCy - 1.5f);
                bolt.lineTo(iconCx + 0.5f, iconCy - 1.5f);
                bolt.closeSubPath();
                g.fillPath(bolt);
            }
            else if (mode == "FUZZ")
            {
                // fuzzy ball: core circle ringed with short irregular
                // spikes -- clearer "fuzzy texture" read than a scribble
                g.fillEllipse(iconCx - 3.2f, iconCy - 3.2f, 6.4f, 6.4f);
                for (int a = 0; a < 8; ++a)
                {
                    float ang = a * juce::MathConstants<float>::twoPi / 8.0f;
                    float len = (a % 2 == 0) ? 4.0f : 2.6f;
                    juce::Point<float> p1(iconCx + std::cos(ang) * 3.2f, iconCy + std::sin(ang) * 3.2f);
                    juce::Point<float> p2(iconCx + std::cos(ang) * (3.2f + len), iconCy + std::sin(ang) * (3.2f + len));
                    g.drawLine({p1, p2}, 1.3f);
                }
            }

            // Small, unnoticeable divider between icon and label.
            float dividerX = groupLeft + iconWidth + dividerGap * 0.5f;
            g.setColour(juce::Colours::white.withAlpha(button.getToggleState() ? 0.18f : 0.10f));
            g.drawLine(dividerX, iconCy - 7.0f, dividerX, iconCy + 7.0f, 1.0f);

            g.setFont(labelFont);
            g.setColour(col);
            float textLeft = groupLeft + iconWidth + dividerGap;
            g.drawText(mode, juce::Rectangle<float>(textLeft, bounds.getY(), textWidth + 2.0f, bounds.getHeight()), juce::Justification::centredLeft);
            return;
        }
        
        // --- NEW FIX: Check button name instead of Unicode text ---
        if (button.getName() == "PRESET_UP" || button.getName() == "PRESET_DOWN")
        {
            juce::Path arrow;
            auto bounds = button.getLocalBounds().toFloat();
            float cx = bounds.getCentreX();
            float cy = bounds.getCentreY();
            float w = 8.0f; // Width of the arrow base
            float h = 6.0f; // Height of the arrow

            if (button.getName() == "PRESET_UP") 
            {
                // Draw flat 2D Up Arrow
                arrow.addTriangle(cx, cy - h/2.0f, cx - w/2.0f, cy + h/2.0f, cx + w/2.0f, cy + h/2.0f);
            }
            else 
            {
                // Draw flat 2D Down Arrow
                arrow.addTriangle(cx - w/2.0f, cy - h/2.0f, cx + w/2.0f, cy - h/2.0f, cx, cy + h/2.0f);
            }

            g.setColour(juce::Colours::white.withAlpha(shouldDrawButtonAsHighlighted ? 1.0f : 0.65f));
            g.fillPath(arrow);
            return; // Exit early so it doesn't try to draw any font
        }
        // ----------------------------------------------------------

        // Standard text rendering for all other buttons
        g.setFont(juce::FontOptions(12.0f).withName("Helvetica").withStyle(button.getToggleState() ? "Bold" : "Plain"));
        
        if (button.getToggleState())
        {
            g.setColour(juce::Colours::white);
            g.drawText(button.getButtonText(), button.getLocalBounds().withTrimmedLeft(6), juce::Justification::centred);
        }
        else
        {
            g.setColour(juce::Colours::white.withAlpha(0.65f));
            g.drawText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred);
        }
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        g.setColour(juce::Colour(0xFF000000).withAlpha(0.5f));
        g.fillRoundedRectangle((float)x, (float)y + (float)height * 0.5f - 2.0f, (float)width, 4.0f, 2.0f);

        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillEllipse(sliderPos - 6.0f, (float)y + (float)height * 0.5f - 4.0f, 12.0f, 12.0f);

        g.setColour(juce::Colours::white);
        g.fillEllipse(sliderPos - 5.0f, (float)y + (float)height * 0.5f - 5.0f, 10.0f, 10.0f);
    }
};

class HomeDistoAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                       public juce::AudioProcessorValueTreeState::Listener
{
public:
    HomeDistoAudioProcessorEditor (HomeDistoAudioProcessor&);
    ~HomeDistoAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    // NEW: the 4-band EQ (LOW cut/shelf, 2 bells, HIGH cut/shelf) is now
    // entirely driven by draggable handles directly on the graph itself
    // (like a real EQ plugin) rather than separate sliders -- these drive
    // that.
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    // NEW: scroll wheel over a bell node adjusts its Q (bandwidth) --
    // standard EQ-plugin convention, keeps the graph from needing a 5th
    // draggable dimension.
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    // NEW: double-click resets an EQ node to baseline (0 dB).
    void mouseDoubleClick (const juce::MouseEvent&) override;

    // FIX: PresetBrowserPanel (a separate class, not a member of this one)
    // needs to call this after loading a preset from its own button
    // callbacks, so it has to be public rather than private.
    void updatePresetName();
    void refreshFromProcessorState();

private:
    HomeDistoAudioProcessor& audioProcessor;
    MinimalistSynthLookAndFeel synthLaf;

    // NEW: AUTO reworked into a UI-layer "linked knobs" feature -- see
    // applyAutoGainCompensation() in the .cpp for the full explanation.
    float lastAutoCompDb = 0.0f;
    float computeAutoCompDb() const;
    void recalibrateAutoBaseline();
    void applyAutoGainCompensation();

    juce::TextButton presetMenuButton;
    juce::TextButton presetUpButton;
    juce::TextButton presetDownButton;
    void showPresetMenu();

    juce::TextButton saveButton;
    juce::TextButton bypassButton;
    juce::TextButton settingsButton;

    juce::Slider driveKnob;
    juce::Slider outputKnob; 
    juce::Slider toneKnob;
    juce::Slider punchKnob;
    juce::Slider mixKnob;
    juce::ToggleButton autoToggle;

    // NEW: lock icons for OUT/MIX. When toggled, switching presets leaves
    // these two knobs exactly where the user left them instead of jumping
    // to the preset's stored value. Not APVTS-backed (see
    // HomeDistoAudioProcessor::lockOutput/lockMix) since this isn't part of
    // "the sound" a host should automate or save -- it's a workflow toggle.
    juce::TextButton outputLockButton;
    juce::TextButton mixLockButton;
    // NEW: EQ reset (flattens all 4 bands to neutral) and EQ lock (like
    // outputLockButton/mixLockButton, but for the whole EQ across presets).
    juce::TextButton eqResetButton;
    juce::TextButton eqLockButton;
    
    juce::TextButton modeButtons[6];
    juce::StringArray modeNames = { "PUNCH", "TUBE", "TAPE", "DIGITAL", "CRUNCH", "FUZZ" };
    
    // REDESIGNED: this is now a 4-band EQ (was a 2-node focus filter).
    // LOW/HIGH can each be Cut or Shelf (click the node to toggle type,
    // drag to move it); BELL1/BELL2 are fully parametric peak bands.
    // All sliders below are still real Sliders purely as the APVTS-attached
    // data model -- invisible, never shown; the graph itself is the control
    // surface. Q for the bell bands is adjusted by mouse wheel over the node
    // (standard EQ-plugin convention) rather than a separate visible control.
    juce::Slider lowFreqSlider, lowGainSlider;
    juce::Slider bell1FreqSlider, bell1GainSlider, bell1QSlider;
    juce::Slider bell2FreqSlider, bell2GainSlider, bell2QSlider;
    juce::Slider highFreqSlider, highGainSlider;

    // Shared geometry for the interactive EQ graph, so painting and
    // hit-testing can never disagree with each other. X is a log-frequency
    // axis shared by ALL bands (not each parameter's own possibly-different
    // skew) so multiple bands with different underlying ranges still line
    // up visually correct against each other; Y is +/-18 dB gain, used for
    // shelf/bell nodes and for drawing the cut-mode roll-off shape.
    static constexpr float filterGraphLeft = 35.0f;
    static constexpr float filterGraphRight = 235.0f;
    // FIX: the graph (and its inset scope border) was clipping right against
    // the card's bottom edge, especially visible on a CUT-mode curve diving
    // all the way to filterGraphBottomY. Moved up a few pixels for
    // breathing room. Also grown further upward now that the on-screen
    // slope buttons (and the row they sat in) are gone -- that space goes
    // to the graph instead.
    static constexpr float filterGraphTopY = 296.0f;    // +18 dB
    static constexpr float filterGraphBottomY = 378.0f; // -18 dB (also the cut-mode floor)
    static constexpr float filterGraphMidY = (filterGraphTopY + filterGraphBottomY) * 0.5f; // 0 dB

    float freqToX(float hz) const;
    float xToFreq(float x) const;
    float gainToY(float db) const;
    float yToGain(float y) const;

    juce::Point<float> lowHandlePos();
    juce::Point<float> highHandlePos();
    juce::Point<float> bell1HandlePos();
    juce::Point<float> bell2HandlePos();

    enum class FilterHandle { none, low, bell1, bell2, high };
    FilterHandle draggingFilterHandle = FilterHandle::none;
    FilterHandle hoveredFilterHandle = FilterHandle::none;
    // Reference point for shift-drag Q/slope adjustment (incremental delta
    // between consecutive drag frames).
    juce::Point<float> lastDragPos;
    // Which button started the current drag. Left is always position, or
    // shift+drag for Q (bells)/slope (LOW/HIGH). Right has no drag gesture
    // at all -- it's used only for a click (not drag) toggling Cut/Shelf on
    // LOW/HIGH, checked in mouseUp. Tracked explicitly here rather than
    // re-checking e.mods in mouseUp, since button state there isn't reliable.
    bool rightButtonDrag = false;
    float slopeDragAccum = 0.0f;
    void toggleBandType (const juce::String& typeParamID);
    void cycleSlope (int direction);

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> driveAttach, outAttach, toneAttach, punchAttach, mixAttach;
    std::unique_ptr<SliderAttachment> lowFreqAttach, lowGainAttach;
    std::unique_ptr<SliderAttachment> bell1FreqAttach, bell1GainAttach, bell1QAttach;
    std::unique_ptr<SliderAttachment> bell2FreqAttach, bell2GainAttach, bell2QAttach;
    std::unique_ptr<SliderAttachment> highFreqAttach, highGainAttach;
    std::unique_ptr<ButtonAttachment> autoAttach;
    std::unique_ptr<ButtonAttachment> bypassAttach;

    juce::String getFrequencyString(float hz);
    void drawShadedCard(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour baseColour);

    // NEW: draws text with per-character spacing computed so the whole
    // string comes out to exactly targetWidth -- used to make the
    // "DUBTACH DSP" credit line match the title's width precisely rather
    // than guessing a number of literal spaces.
    void drawTrackedText(juce::Graphics& g, const juce::String& text, float x, float y, float height, float targetWidth, const juce::Font& font);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HomeDistoAudioProcessorEditor)
};