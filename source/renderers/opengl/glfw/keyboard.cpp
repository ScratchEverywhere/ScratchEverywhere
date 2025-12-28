#include "window.hpp"
#include <interpret.hpp>
#include <keyboard.hpp>
#include <render.hpp>
#include <string>
#include <text.hpp>
#include <vector>

static std::string g_inputText = "";
static bool g_inputActive = false;

static void character_callback(GLFWwindow *window, unsigned int codepoint) {
    if (g_inputActive && codepoint < 128) {
        g_inputText += (char)codepoint;
    }
}

std::string SoftwareKeyboard::openKeyboard(const char *hintText) {
    TextObject *text = createTextObject(std::string(hintText), 0, 0);
    text->setCenterAligned(true);
    text->setColor(Math::color(0, 0, 0, 170));

    TextObject *enterText = createTextObject("ENTER TEXT:", 0, 0);
    enterText->setCenterAligned(true);
    enterText->setColor(Math::color(0, 0, 0, 255));

    g_inputText = "";
    g_inputActive = true;

    GLFWwindow *ctx = (GLFWwindow *)globalWindow->getHandle();
    GLFWcharfun oldCharCallback = glfwSetCharCallback(ctx, character_callback);

    bool backspacePressed = false;

    while (g_inputActive && !glfwWindowShouldClose(ctx)) {
        glfwPollEvents();

        if (glfwGetKey(ctx, GLFW_KEY_ENTER) == GLFW_PRESS ||
            glfwGetKey(ctx, GLFW_KEY_KP_ENTER) == GLFW_PRESS) {
            g_inputActive = false;
        }

        if (glfwGetKey(ctx, GLFW_KEY_BACKSPACE) == GLFW_PRESS) {
            if (!backspacePressed) {
                if (!g_inputText.empty()) {
                    g_inputText.pop_back();
                }
                backspacePressed = true;
            }
        } else {
            backspacePressed = false;
        }

        if (glfwGetKey(ctx, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            g_inputText = "";
            g_inputActive = false;
        }

        if (g_inputText.empty()) {
            text->setText(std::string(hintText));
            text->setColor(Math::color(0, 0, 0, 170));
        } else {
            text->setText(g_inputText);
            text->setColor(Math::color(0, 0, 0, 255));
        }

        text->setScale(1.0f);
        if (text->getSize()[0] > Render::getWidth() * 0.95) {
            float scale = (float)Render::getWidth() / (text->getSize()[0] * 1.05);
            text->setScale(scale);
        }

        Render::beginFrame(0, 117, 77, 117);
        text->render(Render::getWidth() / 2, Render::getHeight() * 0.25);
        enterText->render(Render::getWidth() / 2, Render::getHeight() * 0.15);
        Render::endFrame(false);
    }

    glfwSetCharCallback(ctx, oldCharCallback);
    delete text;
    delete enterText;
    return g_inputText;
}
