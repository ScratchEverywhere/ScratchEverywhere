#include <SDL/SDL.h>
#include <keyboard.hpp>
#include <render.hpp>
#include <string>
#include <text.hpp>

#ifdef __SWITCH__
#include <switch.h>
#endif

/**
 * Uses SDL2 text input.
 */
std::string SoftwareKeyboard::openKeyboard(const char *hintText) {
    TextObject *text = createTextObject(std::string(hintText), 0, 0);
    text->setCenterAligned(true);
    text->setColor(Math::color(0, 0, 0, 170));
    if (text->getSize()[0] > Render::getWidth() * 0.85) {
        float scale = (float)Render::getWidth() / (text->getSize()[0] * 1.15);
        text->setScale(scale);
    }

    TextObject *enterText = createTextObject("ENTER TEXT:", 0, 0);
    enterText->setCenterAligned(true);
    enterText->setColor(Math::color(0, 0, 0, 255));

    std::string inputText = "";
    bool inputActive = true;
    SDL_Event event;

    while (inputActive) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_KEYDOWN:
                // Handle unicode input if available and if it's a printable character
                if (event.key.keysym.unicode != 0 &&
                    (event.key.keysym.unicode & 0xFF80) == 0 && // Basic ASCII check
                    event.key.keysym.unicode >= ' ') {          // Only printable characters
                    inputText += event.key.keysym.unicode;
                    text->setText(inputText);
                    text->setColor(Math::color(0, 0, 0, 255));
                }

                switch (event.key.keysym.sym) {
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    // finish input
                    inputActive = false;
                    break;

                case SDLK_BACKSPACE:
                    // remove character
                    if (!inputText.empty()) {
                        inputText.pop_back();
                    }
                    if (inputText.empty()) {
                        text->setText(std::string(hintText));
                        text->setColor(Math::color(0, 0, 0, 170));
                    } else {
                        text->setText(inputText);
                    }

                    break;

                case SDLK_ESCAPE:
                    // finish input
                    inputActive = false;
                    break;
                }
                break;

            case SDL_QUIT:
                toExit = true;
                inputActive = false;
                break;
            }
        }

        // set text size
        text->setScale(1.0f);
        if (text->getSize()[0] > Render::getWidth() * 0.95) {
            float scale = (float)Render::getWidth() / (text->getSize()[0] * 1.05);
            text->setScale(scale);
        } else {
            text->setScale(1.0f);
        }

        Render::beginFrame(0, 117, 77, 117);

        text->render(Render::getWidth() / 2, Render::getHeight() * 0.25);
        enterText->render(Render::getWidth() / 2, Render::getHeight() * 0.15);

        Render::endFrame(false);
    }

    delete text;
    delete enterText;
    return inputText;

    return "";
}
