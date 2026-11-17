#include <platforms/ut/swkbd_ut.hpp>

#ifdef __UBUNTUTOUCH__

#include <log.hpp>

#include <dbus/dbus.h>

#include <cstdlib>
#include <cstring>

namespace {

constexpr const char *SERVER_PATH = "/com/meego/inputmethod/uiserver1";
constexpr const char *SERVER_INTERFACE = "com.meego.inputmethod.uiserver1";
constexpr const char *CONTEXT_PATH = "/com/meego/inputmethod/inputcontext";
constexpr const char *CONTEXT_INTERFACE = "com.meego.inputmethod.inputcontext1";

constexpr const char *ADDRESS_BUS_NAME = "org.maliit.server";
constexpr const char *ADDRESS_OBJECT_PATH = "/org/maliit/server/address";
constexpr const char *ADDRESS_INTERFACE = "org.maliit.Server.Address";
constexpr const char *ADDRESS_PROPERTY = "address";

// Qt key codes used by Maliit's keyEvent method
constexpr int QT_KEY_ESCAPE = 0x01000000;
constexpr int QT_KEY_BACKSPACE = 0x01000003;
constexpr int QT_KEY_RETURN = 0x01000004;
constexpr int QT_KEY_ENTER = 0x01000005;
constexpr int QT_EVENT_KEY_PRESS = 6;

// Ask the session bus where maliit-server is listening.
// The preferred address comes from the org.maliit.server property;
// MALIIT_SERVER_ADDRESS is only a fallback because the GUID it contains
// goes stale when the server restarts.
std::string fetchMaliitAddress() {
    DBusError error;
    dbus_error_init(&error);

    DBusConnection *bus = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
    if (bus) {
        DBusMessage *message = dbus_message_new_method_call(
            ADDRESS_BUS_NAME, ADDRESS_OBJECT_PATH, "org.freedesktop.DBus.Properties", "Get");
        if (message) {
            DBusMessageIter args;
            dbus_message_iter_init_append(message, &args);
            const char *interface = ADDRESS_INTERFACE;
            const char *property = ADDRESS_PROPERTY;
            dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &interface);
            dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &property);

            DBusMessage *reply = dbus_connection_send_with_reply_and_block(bus, message, 2000, &error);
            dbus_message_unref(message);

            std::string address;
            if (reply) {
                DBusMessageIter iter;
                if (dbus_message_iter_init(reply, &iter) && dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_VARIANT) {
                    DBusMessageIter variant;
                    dbus_message_iter_recurse(&iter, &variant);
                    if (dbus_message_iter_get_arg_type(&variant) == DBUS_TYPE_STRING) {
                        const char *value = nullptr;
                        dbus_message_iter_get_basic(&variant, &value);
                        if (value) {
                            address = value;
                        }
                    }
                }
                dbus_message_unref(reply);
            }

            if (!address.empty()) {
                dbus_connection_close(bus);
                dbus_connection_unref(bus);
                dbus_error_free(&error);
                return address;
            }
        }
        dbus_connection_close(bus);
        dbus_connection_unref(bus);
    }
    dbus_error_free(&error);

    const char *environment = std::getenv("MALIIT_SERVER_ADDRESS");
    return environment ? environment : "";
}

} // namespace

struct UbuntuTouchKeyboard::Impl {
    DBusConnection *connection = nullptr;
    std::string preedit;
    std::vector<Event> events;
    bool available = false;

    ~Impl() { close(); }

    void close() {
        if (!connection) {
            return;
        }
        dbus_connection_close(connection);
        dbus_connection_unref(connection);
        connection = nullptr;
    }

    void push(Event::Type type, const std::string &text = "", int replaceStart = 0, int replaceLength = 0) {
        events.push_back(Event{type, text, replaceStart, replaceLength});
    }

    void sendReply(DBusMessage *reply) {
        if (!reply || !connection) {
            return;
        }
        dbus_connection_send(connection, reply, nullptr);
        dbus_message_unref(reply);
        dbus_connection_flush(connection);
    }

    void replyEmpty(DBusMessage *call) { sendReply(dbus_message_new_method_return(call)); }

    void replySelection(DBusMessage *call) {
        DBusMessage *reply = dbus_message_new_method_return(call);
        if (!reply) {
            return;
        }
        DBusMessageIter args;
        dbus_message_iter_init_append(reply, &args);
        dbus_bool_t valid = FALSE;
        const char *text = "";
        dbus_message_iter_append_basic(&args, DBUS_TYPE_BOOLEAN, &valid);
        dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &text);
        sendReply(reply);
    }

    void replyPreeditRectangle(DBusMessage *call) {
        DBusMessage *reply = dbus_message_new_method_return(call);
        if (!reply) {
            return;
        }
        DBusMessageIter args;
        dbus_message_iter_init_append(reply, &args);
        dbus_bool_t valid = FALSE;
        dbus_int32_t zero = 0;
        dbus_message_iter_append_basic(&args, DBUS_TYPE_BOOLEAN, &valid);
        for (int i = 0; i < 4; ++i) {
            dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &zero);
        }
        sendReply(reply);
    }

    static DBusHandlerResult messageHandler(DBusConnection *, DBusMessage *message, void *userData) {
        if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_METHOD_CALL) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }

        const char *interface = dbus_message_get_interface(message);
        if (interface && std::strcmp(interface, CONTEXT_INTERFACE) != 0) {
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
        }

        static_cast<Impl *>(userData)->handleMethodCall(message);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    void handleMethodCall(DBusMessage *message) {
        const char *member = dbus_message_get_member(message);
        if (!member) {
            replyEmpty(message);
            return;
        }

        if (std::strcmp(member, "commitString") == 0) {
            DBusMessageIter iter;
            std::string text;
            int replaceStart = 0;
            int replaceLength = 0;
            if (dbus_message_iter_init(message, &iter)) {
                if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
                    const char *value = nullptr;
                    dbus_message_iter_get_basic(&iter, &value);
                    if (value) {
                        text = value;
                    }
                }
                dbus_message_iter_next(&iter);
                if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_INT32) {
                    dbus_message_iter_get_basic(&iter, &replaceStart);
                }
                dbus_message_iter_next(&iter);
                if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_INT32) {
                    dbus_message_iter_get_basic(&iter, &replaceLength);
                }
            }

            preedit.clear();

            // Some layouts send the return key as committed text.
            if (text == "\n" || text == "\r") {
                push(Event::Type::Enter);
            } else {
                push(Event::Type::CommitString, text, replaceStart, replaceLength);
            }
            replyEmpty(message);
        } else if (std::strcmp(member, "updatePreedit") == 0) {
            DBusMessageIter iter;
            std::string text;
            if (dbus_message_iter_init(message, &iter) && dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_STRING) {
                const char *value = nullptr;
                dbus_message_iter_get_basic(&iter, &value);
                if (value) {
                    text = value;
                }
            }
            preedit = text;
            push(Event::Type::Preedit, text);
            replyEmpty(message);
        } else if (std::strcmp(member, "keyEvent") == 0) {
            DBusMessageIter iter;
            int type = 0;
            int key = 0;
            if (dbus_message_iter_init(message, &iter)) {
                if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_INT32) {
                    dbus_message_iter_get_basic(&iter, &type);
                }
                dbus_message_iter_next(&iter);
                if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_INT32) {
                    dbus_message_iter_get_basic(&iter, &key);
                }
            }

            if (type == QT_EVENT_KEY_PRESS) {
                switch (key) {
                case QT_KEY_BACKSPACE:
                    push(Event::Type::Backspace);
                    break;
                case QT_KEY_RETURN:
                case QT_KEY_ENTER:
                    push(Event::Type::Enter);
                    break;
                case QT_KEY_ESCAPE:
                    push(Event::Type::Escape);
                    break;
                default:
                    break;
                }
            }
            replyEmpty(message);
        } else if (std::strcmp(member, "updateInputMethodArea") == 0) {
            // Lomiri resizes our surface itself, so the reported area is only informational for this dialog.
            replyEmpty(message);
        } else if (std::strcmp(member, "imInitiatedHide") == 0) {
            preedit.clear();
            push(Event::Type::Dismissed);
            replyEmpty(message);
        } else if (std::strcmp(member, "activationLostEvent") == 0) {
            preedit.clear();
            replyEmpty(message);
        } else if (std::strcmp(member, "selection") == 0) {
            replySelection(message);
        } else if (std::strcmp(member, "preeditRectangle") == 0) {
            replyPreeditRectangle(message);
        } else {
            replyEmpty(message);
        }
    }

    void sendSimple(const char *method) {
        if (!connection) {
            return;
        }
        DBusMessage *message = dbus_message_new_method_call(nullptr, SERVER_PATH, SERVER_INTERFACE, method);
        if (!message) {
            return;
        }
        dbus_connection_send(connection, message, nullptr);
        dbus_message_unref(message);
        dbus_connection_flush(connection);
    }

    void sendWidgetInfo() {
        if (!connection) {
            return;
        }
        DBusMessage *message = dbus_message_new_method_call(nullptr, SERVER_PATH, SERVER_INTERFACE, "updateWidgetInformation");
        if (!message) {
            return;
        }

        DBusMessageIter args;
        DBusMessageIter properties;
        dbus_message_iter_init_append(message, &args);
        dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &properties);
        dbus_message_iter_close_container(&args, &properties);
        dbus_bool_t focus = TRUE;
        dbus_message_iter_append_basic(&args, DBUS_TYPE_BOOLEAN, &focus);

        dbus_connection_send(connection, message, nullptr);
        dbus_message_unref(message);
        dbus_connection_flush(connection);
    }

    void show() {
        if (!connection) {
            return;
        }
        sendSimple("activateContext");
        sendWidgetInfo();
        sendSimple("showInputMethod");
    }

    void hide() {
        if (!connection) {
            return;
        }
        sendSimple("activateContext");
        sendSimple("hideInputMethod");
        preedit.clear();
    }

    void poll() {
        if (!connection) {
            return;
        }
        if (!dbus_connection_read_write(connection, 0)) {
            Log::logWarning("Maliit: on-screen keyboard connection closed");
            close();
            available = false;
            return;
        }
        while (dbus_connection_dispatch(connection) == DBUS_DISPATCH_DATA_REMAINS) {
        }
    }
};

UbuntuTouchKeyboard::UbuntuTouchKeyboard() {
    dbus_threads_init_default();
    impl = new Impl();

    static const DBusObjectPathVTable contextVTable = {
        nullptr,
        &Impl::messageHandler,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
    };

    const std::string address = fetchMaliitAddress();
    if (address.empty()) {
        Log::logWarning("Maliit: no on-screen keyboard bus address found");
        return;
    }

    DBusError error;
    dbus_error_init(&error);
    impl->connection = dbus_connection_open_private(address.c_str(), &error);
    if (!impl->connection) {
        Log::logWarning("Maliit: unable to connect to the on-screen keyboard bus");
        dbus_error_free(&error);
        return;
    }
    dbus_error_free(&error);

    dbus_connection_set_exit_on_disconnect(impl->connection, FALSE);
    dbus_connection_set_route_peer_messages(impl->connection, TRUE);

    if (!dbus_connection_register_object_path(impl->connection, CONTEXT_PATH, &contextVTable, impl)) {
        Log::logWarning("Maliit: unable to register the input context object");
        impl->close();
        return;
    }

    impl->available = true;
}

UbuntuTouchKeyboard::~UbuntuTouchKeyboard() { delete impl; }

bool UbuntuTouchKeyboard::isAvailable() const { return impl && impl->available && impl->connection; }

const std::string &UbuntuTouchKeyboard::preeditText() const {
    static const std::string empty;
    return impl ? impl->preedit : empty;
}

void UbuntuTouchKeyboard::show() {
    if (impl) {
        impl->show();
    }
}

void UbuntuTouchKeyboard::hide() {
    if (impl) {
        impl->hide();
    }
}

void UbuntuTouchKeyboard::poll() {
    if (impl) {
        impl->poll();
    }
}

std::vector<UbuntuTouchKeyboard::Event> UbuntuTouchKeyboard::takeEvents() {
    std::vector<Event> events;
    if (impl) {
        events.swap(impl->events);
    }
    return events;
}

#else

struct UbuntuTouchKeyboard::Impl {};

UbuntuTouchKeyboard::UbuntuTouchKeyboard() {}
UbuntuTouchKeyboard::~UbuntuTouchKeyboard() {}

bool UbuntuTouchKeyboard::isAvailable() const { return false; }

const std::string &UbuntuTouchKeyboard::preeditText() const {
    static const std::string empty;
    return empty;
}

void UbuntuTouchKeyboard::show() {}
void UbuntuTouchKeyboard::hide() {}
void UbuntuTouchKeyboard::poll() {}

std::vector<UbuntuTouchKeyboard::Event> UbuntuTouchKeyboard::takeEvents() { return {}; }

#endif
