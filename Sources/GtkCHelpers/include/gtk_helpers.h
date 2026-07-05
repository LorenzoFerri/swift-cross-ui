#ifndef __GTK_HELPERS_H__
#define __GTK_HELPERS_H__

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

GtkWidget *wrapped_gtk_message_dialog_new(void);

typedef struct SCUIStatusNotifierItem SCUIStatusNotifierItem;
typedef void (*SCUIStatusNotifierActivateCallback)(void *user_data);
typedef void (*SCUIStatusNotifierMenuItemCallback)(int item_id, void *user_data);

typedef struct {
    int id;
    int parent_id;
    const char *label;
    int enabled;
    int visible;
    int is_separator;
    int is_toggle;
    int toggle_state;
} SCUIStatusNotifierMenuItem;

SCUIStatusNotifierItem *scui_status_notifier_item_new(
    const char *id,
    const char *title,
    SCUIStatusNotifierActivateCallback activate_callback,
    void *user_data
);

void scui_status_notifier_item_update(
    SCUIStatusNotifierItem *item,
    const char *title,
    const char *icon_name,
    const char *icon_path,
    const char *icon_file,
    const char *tooltip
);

void scui_status_notifier_item_set_menu(
    SCUIStatusNotifierItem *item,
    const SCUIStatusNotifierMenuItem *menu_items,
    int menu_item_count,
    SCUIStatusNotifierMenuItemCallback menu_item_callback,
    void *user_data
);

void scui_status_notifier_item_free(SCUIStatusNotifierItem *item);

// Swift suddenly stopped finding these corresponding `G_*` enum members on its
// own on macOS. Weirdly everything worked in one command run, and then it started
// failing in the next (with identical code). Then when I tried recreating the
// issue on my Mac I could, even though I successfully built Gtk/Gtk3 a few days
// earlier... I'm perplexed, but this does at least solve the issue
extern const GConnectFlags SHIM_G_CONNECT_AFTER;
extern const GConnectFlags SHIM_G_CONNECT_SWAPPED;
extern const GApplicationFlags SHIM_G_APPLICATION_HANDLES_OPEN;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTK_HELPERS_H__ */
