#ifndef _XBOX_LIBDLGMOD_H
#define _XBOX_LIBDLGMOD_H
static inline int DialogMessage(const char *title, const char *msg, const char *type) { return 0; }
static inline const char *widget_get_caption() { return ""; }
static inline const char *widget_get_button_name(int btn) { return ""; }
static inline void widget_set_caption(const char *cap) {}
static inline void widget_set_button_name(int btn, const char *name) {}
static inline void show_error(const char *msg, bool fatal) {}
static inline const char *get_directory_alt(const char *title, const char *path) { return ""; }
#endif
