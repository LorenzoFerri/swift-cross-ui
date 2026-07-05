#include "gtk_helpers.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

GtkWidget *wrapped_gtk_message_dialog_new() {
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wdeprecated-declarations"
    return gtk_message_dialog_new(
        NULL,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO,
        GTK_BUTTONS_NONE,
        ""
    );
    #pragma clang diagnostic pop
}

const GConnectFlags SHIM_G_CONNECT_AFTER = G_CONNECT_AFTER;
const GConnectFlags SHIM_G_CONNECT_SWAPPED = G_CONNECT_AFTER;
const GApplicationFlags SHIM_G_APPLICATION_HANDLES_OPEN = G_APPLICATION_HANDLES_OPEN;

struct SCUIStatusNotifierItem {
    char *id;
    char *title;
    char *icon_name;
    char *icon_path;
    char *icon_file;
    char *tooltip;
    char *bus_name;

    GDBusConnection *connection;
    guint bus_owner_id;
    guint object_registration_id;
    guint menu_registration_id;

    SCUIStatusNotifierActivateCallback activate_callback;
    void *user_data;
    SCUIStatusNotifierMenuItem *menu_items;
    int menu_item_count;
    guint menu_revision;
    SCUIStatusNotifierMenuItemCallback menu_item_callback;
    void *menu_item_user_data;
};

static const char *scui_sni_introspection_xml =
    "<node>"
    "  <interface name='org.kde.StatusNotifierItem'>"
    "    <method name='ContextMenu'>"
    "      <arg name='x' type='i' direction='in'/>"
    "      <arg name='y' type='i' direction='in'/>"
    "    </method>"
    "    <method name='Activate'>"
    "      <arg name='x' type='i' direction='in'/>"
    "      <arg name='y' type='i' direction='in'/>"
    "    </method>"
    "    <method name='SecondaryActivate'>"
    "      <arg name='x' type='i' direction='in'/>"
    "      <arg name='y' type='i' direction='in'/>"
    "    </method>"
    "    <method name='Scroll'>"
    "      <arg name='delta' type='i' direction='in'/>"
    "      <arg name='orientation' type='s' direction='in'/>"
    "    </method>"
    "    <property name='Category' type='s' access='read'/>"
    "    <property name='Id' type='s' access='read'/>"
    "    <property name='Title' type='s' access='read'/>"
    "    <property name='Status' type='s' access='read'/>"
    "    <property name='WindowId' type='i' access='read'/>"
    "    <property name='IconName' type='s' access='read'/>"
    "    <property name='IconThemePath' type='s' access='read'/>"
    "    <property name='IconPixmap' type='a(iiay)' access='read'/>"
    "    <property name='OverlayIconName' type='s' access='read'/>"
    "    <property name='OverlayIconPixmap' type='a(iiay)' access='read'/>"
    "    <property name='AttentionIconName' type='s' access='read'/>"
    "    <property name='AttentionIconPixmap' type='a(iiay)' access='read'/>"
    "    <property name='AttentionMovieName' type='s' access='read'/>"
    "    <property name='ToolTip' type='(sa(iiay)ss)' access='read'/>"
    "    <property name='ItemIsMenu' type='b' access='read'/>"
    "    <property name='Menu' type='o' access='read'/>"
    "    <signal name='NewTitle'/>"
    "    <signal name='NewIcon'/>"
    "    <signal name='NewToolTip'/>"
    "    <signal name='NewStatus'>"
    "      <arg name='status' type='s'/>"
    "    </signal>"
    "  </interface>"
    "</node>";

static GDBusNodeInfo *scui_sni_introspection_data = NULL;

static const char *scui_dbusmenu_introspection_xml =
    "<node>"
    "  <interface name='com.canonical.dbusmenu'>"
    "    <method name='GetLayout'>"
    "      <arg name='parentId' type='i' direction='in'/>"
    "      <arg name='recursionDepth' type='i' direction='in'/>"
    "      <arg name='propertyNames' type='as' direction='in'/>"
    "      <arg name='revision' type='u' direction='out'/>"
    "      <arg name='layout' type='(ia{sv}av)' direction='out'/>"
    "    </method>"
    "    <method name='GetGroupProperties'>"
    "      <arg name='ids' type='ai' direction='in'/>"
    "      <arg name='propertyNames' type='as' direction='in'/>"
    "      <arg name='properties' type='a(ia{sv})' direction='out'/>"
    "    </method>"
    "    <method name='Event'>"
    "      <arg name='id' type='i' direction='in'/>"
    "      <arg name='eventId' type='s' direction='in'/>"
    "      <arg name='data' type='v' direction='in'/>"
    "      <arg name='timestamp' type='u' direction='in'/>"
    "    </method>"
    "    <method name='AboutToShow'>"
    "      <arg name='id' type='i' direction='in'/>"
    "      <arg name='needUpdate' type='b' direction='out'/>"
    "    </method>"
    "    <signal name='LayoutUpdated'>"
    "      <arg name='revision' type='u'/>"
    "      <arg name='parent' type='i'/>"
    "    </signal>"
    "    <property name='Version' type='u' access='read'/>"
    "    <property name='TextDirection' type='s' access='read'/>"
    "    <property name='Status' type='s' access='read'/>"
    "    <property name='IconThemePath' type='as' access='read'/>"
    "  </interface>"
    "</node>";

static GDBusNodeInfo *scui_dbusmenu_introspection_data = NULL;

static char *scui_strdup_or_empty(const char *value) {
    return g_strdup(value ? value : "");
}

static void scui_replace_string(char **destination, const char *value) {
    g_free(*destination);
    *destination = scui_strdup_or_empty(value);
}

static char *scui_sni_sanitize_bus_name_component(const char *value) {
    const char *source = value ? value : "";
    if (source[0] == '\0') {
        return g_strdup("status_item");
    }

    char *sanitized = g_strdup(source);
    for (char *cursor = sanitized; *cursor != '\0'; cursor++) {
        unsigned char character = (unsigned char)*cursor;
        if (!g_ascii_isalnum(character) && character != '_' && character != '-') {
            *cursor = '_';
        }
    }
    return sanitized;
}

static GVariant *scui_empty_icon_pixmap(void) {
    GVariantBuilder builder;
    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(iiay)"));
    return g_variant_builder_end(&builder);
}

static gboolean scui_add_icon_pixmap_for_size(
    GVariantBuilder *pixmaps,
    const char *path,
    int size
) {
    GError *error = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file_at_scale(
        path,
        size,
        size,
        TRUE,
        &error
    );
    if (!pixbuf) {
        if (error) {
            g_error_free(error);
        }
        return FALSE;
    }

    int width = gdk_pixbuf_get_width(pixbuf);
    int height = gdk_pixbuf_get_height(pixbuf);
    int rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    int channels = gdk_pixbuf_get_n_channels(pixbuf);
    gboolean has_alpha = gdk_pixbuf_get_has_alpha(pixbuf);
    guchar *pixels = gdk_pixbuf_get_pixels(pixbuf);

    GVariantBuilder bytes;
    g_variant_builder_init(&bytes, G_VARIANT_TYPE("ay"));

    for (int y = 0; y < height; y++) {
        guchar *row = pixels + y * rowstride;
        for (int x = 0; x < width; x++) {
            guchar *pixel = row + x * channels;
            guint8 red = pixel[0];
            guint8 green = pixel[1];
            guint8 blue = pixel[2];
            guint8 alpha = has_alpha ? pixel[3] : 0xff;

            g_variant_builder_add(&bytes, "y", alpha);
            g_variant_builder_add(&bytes, "y", red);
            g_variant_builder_add(&bytes, "y", green);
            g_variant_builder_add(&bytes, "y", blue);
        }
    }

    g_variant_builder_add(
        pixmaps,
        "(ii@ay)",
        width,
        height,
        g_variant_builder_end(&bytes)
    );
    g_object_unref(pixbuf);
    return TRUE;
}

static GVariant *scui_icon_pixmap_from_file(const char *path) {
    static const int sizes[] = { 16, 22, 24, 32, 44, 48, 64 };

    GVariantBuilder pixmaps;
    g_variant_builder_init(&pixmaps, G_VARIANT_TYPE("a(iiay)"));

    for (gsize i = 0; i < G_N_ELEMENTS(sizes); i++) {
        scui_add_icon_pixmap_for_size(&pixmaps, path, sizes[i]);
    }

    return g_variant_builder_end(&pixmaps);
}

static GVariant *scui_icon_pixmap(SCUIStatusNotifierItem *item) {
    if (item->icon_file && item->icon_file[0] != '\0') {
        return scui_icon_pixmap_from_file(item->icon_file);
    }
    return scui_empty_icon_pixmap();
}

static GVariant *scui_sni_get_property(
    GDBusConnection *connection,
    const gchar *sender,
    const gchar *object_path,
    const gchar *interface_name,
    const gchar *property_name,
    GError **error,
    gpointer user_data
) {
    SCUIStatusNotifierItem *item = user_data;

    if (g_strcmp0(property_name, "Category") == 0) {
        return g_variant_new_string("ApplicationStatus");
    } else if (g_strcmp0(property_name, "Id") == 0) {
        return g_variant_new_string(item->id);
    } else if (g_strcmp0(property_name, "Title") == 0) {
        return g_variant_new_string(item->title);
    } else if (g_strcmp0(property_name, "Status") == 0) {
        return g_variant_new_string("Active");
    } else if (g_strcmp0(property_name, "WindowId") == 0) {
        return g_variant_new_int32(0);
    } else if (g_strcmp0(property_name, "IconName") == 0) {
        return g_variant_new_string(item->icon_name);
    } else if (g_strcmp0(property_name, "IconThemePath") == 0) {
        return g_variant_new_string(item->icon_path);
    } else if (g_strcmp0(property_name, "IconPixmap") == 0) {
        return scui_icon_pixmap(item);
    } else if (g_strcmp0(property_name, "OverlayIconName") == 0) {
        return g_variant_new_string("");
    } else if (g_strcmp0(property_name, "OverlayIconPixmap") == 0) {
        return scui_empty_icon_pixmap();
    } else if (g_strcmp0(property_name, "AttentionIconName") == 0) {
        return g_variant_new_string("");
    } else if (g_strcmp0(property_name, "AttentionIconPixmap") == 0) {
        return scui_empty_icon_pixmap();
    } else if (g_strcmp0(property_name, "AttentionMovieName") == 0) {
        return g_variant_new_string("");
    } else if (g_strcmp0(property_name, "ToolTip") == 0) {
        return g_variant_new(
            "(s@a(iiay)ss)",
            item->icon_name,
            scui_icon_pixmap(item),
            item->title,
            item->tooltip
        );
    } else if (g_strcmp0(property_name, "ItemIsMenu") == 0) {
        return g_variant_new_boolean(item->menu_item_count > 0);
    } else if (g_strcmp0(property_name, "Menu") == 0) {
        return g_variant_new_object_path("/Menu");
    }

    g_set_error(
        error,
        G_IO_ERROR,
        G_IO_ERROR_NOT_SUPPORTED,
        "Unknown StatusNotifierItem property '%s'",
        property_name
    );
    return NULL;
}

static void scui_sni_handle_method_call(
    GDBusConnection *connection,
    const gchar *sender,
    const gchar *object_path,
    const gchar *interface_name,
    const gchar *method_name,
    GVariant *parameters,
    GDBusMethodInvocation *invocation,
    gpointer user_data
) {
    SCUIStatusNotifierItem *item = user_data;

    if (g_strcmp0(method_name, "ContextMenu") == 0) {
        if (item->menu_item_count == 0 && item->activate_callback) {
            item->activate_callback(item->user_data);
        }
        g_dbus_method_invocation_return_value(invocation, NULL);
        return;
    }

    if (
        g_strcmp0(method_name, "Activate") == 0
        || g_strcmp0(method_name, "SecondaryActivate") == 0
    ) {
        if (item->activate_callback) {
            item->activate_callback(item->user_data);
        }
        g_dbus_method_invocation_return_value(invocation, NULL);
        return;
    }

    if (g_strcmp0(method_name, "Scroll") == 0) {
        g_dbus_method_invocation_return_value(invocation, NULL);
        return;
    }

    g_dbus_method_invocation_return_error(
        invocation,
        G_IO_ERROR,
        G_IO_ERROR_NOT_SUPPORTED,
        "Unknown StatusNotifierItem method '%s'",
        method_name
    );
}

static const GDBusInterfaceVTable scui_sni_vtable = {
    scui_sni_handle_method_call,
    scui_sni_get_property,
    NULL,
};

static SCUIStatusNotifierMenuItem *scui_dbusmenu_find_item(
    SCUIStatusNotifierItem *item,
    int item_id
) {
    for (int i = 0; i < item->menu_item_count; i++) {
        if (item->menu_items[i].id == item_id) {
            return &item->menu_items[i];
        }
    }
    return NULL;
}

static void scui_dbusmenu_add_properties(
    SCUIStatusNotifierMenuItem *menu_item,
    GVariantBuilder *properties
) {
    if (!menu_item) {
        g_variant_builder_add(
            properties,
            "{sv}",
            "children-display",
            g_variant_new_string("submenu")
        );
        return;
    }

    if (menu_item->is_separator) {
        g_variant_builder_add(
            properties,
            "{sv}",
            "type",
            g_variant_new_string("separator")
        );
    } else {
        g_variant_builder_add(
            properties,
            "{sv}",
            "label",
            g_variant_new_string(menu_item->label ? menu_item->label : "")
        );
    }

    g_variant_builder_add(
        properties,
        "{sv}",
        "enabled",
        g_variant_new_boolean(menu_item->enabled)
    );
    g_variant_builder_add(
        properties,
        "{sv}",
        "visible",
        g_variant_new_boolean(menu_item->visible)
    );

    if (menu_item->is_toggle) {
        g_variant_builder_add(
            properties,
            "{sv}",
            "toggle-type",
            g_variant_new_string("checkmark")
        );
        g_variant_builder_add(
            properties,
            "{sv}",
            "toggle-state",
            g_variant_new_int32(menu_item->toggle_state ? 1 : 0)
        );
    }
}

static GVariant *scui_dbusmenu_build_layout(SCUIStatusNotifierItem *item, int parent_id) {
    SCUIStatusNotifierMenuItem *menu_item =
        parent_id == 0 ? NULL : scui_dbusmenu_find_item(item, parent_id);

    GVariantBuilder properties;
    g_variant_builder_init(&properties, G_VARIANT_TYPE("a{sv}"));
    scui_dbusmenu_add_properties(menu_item, &properties);

    GVariantBuilder children;
    g_variant_builder_init(&children, G_VARIANT_TYPE("av"));

    for (int i = 0; i < item->menu_item_count; i++) {
        if (item->menu_items[i].parent_id == parent_id) {
            g_variant_builder_add(
                &children,
                "v",
                scui_dbusmenu_build_layout(item, item->menu_items[i].id)
            );
        }
    }

    return g_variant_new(
        "(i@a{sv}@av)",
        parent_id,
        g_variant_builder_end(&properties),
        g_variant_builder_end(&children)
    );
}

static GVariant *scui_dbusmenu_build_group_properties(
    SCUIStatusNotifierItem *item,
    GVariant *ids
) {
    GVariantBuilder result;
    g_variant_builder_init(&result, G_VARIANT_TYPE("a(ia{sv})"));

    GVariantIter iter;
    gint32 id;
    g_variant_iter_init(&iter, ids);
    while (g_variant_iter_next(&iter, "i", &id)) {
        SCUIStatusNotifierMenuItem *menu_item = scui_dbusmenu_find_item(item, id);
        if (!menu_item) {
            continue;
        }

        GVariantBuilder properties;
        g_variant_builder_init(&properties, G_VARIANT_TYPE("a{sv}"));
        scui_dbusmenu_add_properties(menu_item, &properties);
        g_variant_builder_add(
            &result,
            "(i@a{sv})",
            id,
            g_variant_builder_end(&properties)
        );
    }

    return g_variant_builder_end(&result);
}

static void scui_dbusmenu_handle_method_call(
    GDBusConnection *connection,
    const gchar *sender,
    const gchar *object_path,
    const gchar *interface_name,
    const gchar *method_name,
    GVariant *parameters,
    GDBusMethodInvocation *invocation,
    gpointer user_data
) {
    SCUIStatusNotifierItem *item = user_data;

    if (g_strcmp0(method_name, "GetLayout") == 0) {
        gint32 parent_id = 0;
        gint32 recursion_depth = 0;
        GVariant *property_names = NULL;
        g_variant_get(parameters, "(ii@as)", &parent_id, &recursion_depth, &property_names);
        if (property_names) {
            g_variant_unref(property_names);
        }

        GVariant *layout = scui_dbusmenu_build_layout(item, parent_id);
        g_dbus_method_invocation_return_value(
            invocation,
            g_variant_new("(u@(ia{sv}av))", item->menu_revision, layout)
        );
        return;
    }

    if (g_strcmp0(method_name, "GetGroupProperties") == 0) {
        GVariant *ids = NULL;
        GVariant *property_names = NULL;
        g_variant_get(parameters, "(@ai@as)", &ids, &property_names);
        if (property_names) {
            g_variant_unref(property_names);
        }

        GVariant *properties = scui_dbusmenu_build_group_properties(item, ids);
        if (ids) {
            g_variant_unref(ids);
        }
        g_dbus_method_invocation_return_value(
            invocation,
            g_variant_new("(@a(ia{sv}))", properties)
        );
        return;
    }

    if (g_strcmp0(method_name, "Event") == 0) {
        gint32 id = 0;
        const gchar *event_id = NULL;
        GVariant *data = NULL;
        guint32 timestamp = 0;
        g_variant_get(parameters, "(is@vu)", &id, &event_id, &data, &timestamp);
        if (data) {
            g_variant_unref(data);
        }

        if (
            g_strcmp0(event_id, "clicked") == 0
            && item->menu_item_callback
            && scui_dbusmenu_find_item(item, id)
        ) {
            item->menu_item_callback(id, item->menu_item_user_data);
        }

        g_dbus_method_invocation_return_value(invocation, NULL);
        return;
    }

    if (g_strcmp0(method_name, "AboutToShow") == 0) {
        g_dbus_method_invocation_return_value(invocation, g_variant_new("(b)", FALSE));
        return;
    }

    g_dbus_method_invocation_return_error(
        invocation,
        G_IO_ERROR,
        G_IO_ERROR_NOT_SUPPORTED,
        "Unknown DBusMenu method '%s'",
        method_name
    );
}

static GVariant *scui_dbusmenu_get_property(
    GDBusConnection *connection,
    const gchar *sender,
    const gchar *object_path,
    const gchar *interface_name,
    const gchar *property_name,
    GError **error,
    gpointer user_data
) {
    if (g_strcmp0(property_name, "Version") == 0) {
        return g_variant_new_uint32(3);
    } else if (g_strcmp0(property_name, "TextDirection") == 0) {
        return g_variant_new_string("ltr");
    } else if (g_strcmp0(property_name, "Status") == 0) {
        return g_variant_new_string("normal");
    } else if (g_strcmp0(property_name, "IconThemePath") == 0) {
        GVariantBuilder builder;
        g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
        return g_variant_builder_end(&builder);
    }

    g_set_error(
        error,
        G_IO_ERROR,
        G_IO_ERROR_NOT_SUPPORTED,
        "Unknown DBusMenu property '%s'",
        property_name
    );
    return NULL;
}

static const GDBusInterfaceVTable scui_dbusmenu_vtable = {
    scui_dbusmenu_handle_method_call,
    scui_dbusmenu_get_property,
    NULL,
};

static void scui_sni_emit_signal(SCUIStatusNotifierItem *item, const char *signal_name) {
    if (!item->connection) {
        return;
    }

    g_dbus_connection_emit_signal(
        item->connection,
        NULL,
        "/StatusNotifierItem",
        "org.kde.StatusNotifierItem",
        signal_name,
        NULL,
        NULL
    );
}

SCUIStatusNotifierItem *scui_status_notifier_item_new(
    const char *id,
    const char *title,
    SCUIStatusNotifierActivateCallback activate_callback,
    void *user_data
) {
    GError *error = NULL;

    if (!scui_sni_introspection_data) {
        scui_sni_introspection_data = g_dbus_node_info_new_for_xml(
            scui_sni_introspection_xml,
            &error
        );
        if (error) {
            g_warning("Failed to parse StatusNotifierItem introspection XML: %s", error->message);
            g_error_free(error);
            return NULL;
        }
    }
    if (!scui_dbusmenu_introspection_data) {
        scui_dbusmenu_introspection_data = g_dbus_node_info_new_for_xml(
            scui_dbusmenu_introspection_xml,
            &error
        );
        if (error) {
            g_warning("Failed to parse DBusMenu introspection XML: %s", error->message);
            g_error_free(error);
            return NULL;
        }
    }

    SCUIStatusNotifierItem *item = g_new0(SCUIStatusNotifierItem, 1);
    item->id = scui_strdup_or_empty(id);
    item->title = scui_strdup_or_empty(title);
    item->icon_name = scui_strdup_or_empty(NULL);
    item->icon_path = scui_strdup_or_empty(NULL);
    item->icon_file = scui_strdup_or_empty(NULL);
    item->tooltip = scui_strdup_or_empty(NULL);
    item->activate_callback = activate_callback;
    item->user_data = user_data;
    char *bus_name_component = scui_sni_sanitize_bus_name_component(item->id);
    item->bus_name = g_strdup_printf(
        "org.kde.StatusNotifierItem-%ld-%s",
        (long)getpid(),
        bus_name_component
    );
    g_free(bus_name_component);

    item->connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (error) {
        g_warning("Failed to connect to the session bus for StatusNotifierItem: %s", error->message);
        g_error_free(error);
        scui_status_notifier_item_free(item);
        return NULL;
    }

    item->object_registration_id = g_dbus_connection_register_object(
        item->connection,
        "/StatusNotifierItem",
        scui_sni_introspection_data->interfaces[0],
        &scui_sni_vtable,
        item,
        NULL,
        &error
    );
    if (error) {
        g_warning("Failed to register StatusNotifierItem object: %s", error->message);
        g_error_free(error);
        scui_status_notifier_item_free(item);
        return NULL;
    }

    item->menu_registration_id = g_dbus_connection_register_object(
        item->connection,
        "/Menu",
        scui_dbusmenu_introspection_data->interfaces[0],
        &scui_dbusmenu_vtable,
        item,
        NULL,
        &error
    );
    if (error) {
        g_warning("Failed to register DBusMenu object: %s", error->message);
        g_error_free(error);
        scui_status_notifier_item_free(item);
        return NULL;
    }

    item->bus_owner_id = g_bus_own_name_on_connection(
        item->connection,
        item->bus_name,
        G_BUS_NAME_OWNER_FLAGS_NONE,
        NULL,
        NULL,
        NULL,
        NULL
    );

    GVariant *registration_result = g_dbus_connection_call_sync(
        item->connection,
        "org.kde.StatusNotifierWatcher",
        "/StatusNotifierWatcher",
        "org.kde.StatusNotifierWatcher",
        "RegisterStatusNotifierItem",
        g_variant_new("(s)", item->bus_name),
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );
    if (error) {
        g_warning(
            "Failed to register StatusNotifierItem with org.kde.StatusNotifierWatcher: %s",
            error->message
        );
        g_error_free(error);
    }
    if (registration_result) {
        g_variant_unref(registration_result);
    }

    return item;
}

void scui_status_notifier_item_update(
    SCUIStatusNotifierItem *item,
    const char *title,
    const char *icon_name,
    const char *icon_path,
    const char *icon_file,
    const char *tooltip
) {
    if (!item) {
        return;
    }

    gboolean title_changed = g_strcmp0(item->title, title ? title : "") != 0;
    gboolean icon_changed =
        g_strcmp0(item->icon_name, icon_name ? icon_name : "") != 0
        || g_strcmp0(item->icon_path, icon_path ? icon_path : "") != 0
        || g_strcmp0(item->icon_file, icon_file ? icon_file : "") != 0;
    gboolean tooltip_changed = g_strcmp0(item->tooltip, tooltip ? tooltip : "") != 0;

    scui_replace_string(&item->title, title);
    scui_replace_string(&item->icon_name, icon_name);
    scui_replace_string(&item->icon_path, icon_path);
    scui_replace_string(&item->icon_file, icon_file);
    scui_replace_string(&item->tooltip, tooltip);

    if (title_changed) {
        scui_sni_emit_signal(item, "NewTitle");
    }
    if (icon_changed) {
        scui_sni_emit_signal(item, "NewIcon");
    }
    if (tooltip_changed) {
        scui_sni_emit_signal(item, "NewToolTip");
    }
}

void scui_status_notifier_item_set_menu(
    SCUIStatusNotifierItem *item,
    const SCUIStatusNotifierMenuItem *menu_items,
    int menu_item_count,
    SCUIStatusNotifierMenuItemCallback menu_item_callback,
    void *user_data
) {
    if (!item) {
        return;
    }

    for (int i = 0; i < item->menu_item_count; i++) {
        g_free((char *)item->menu_items[i].label);
    }
    g_free(item->menu_items);

    item->menu_items = NULL;
    item->menu_item_count = MAX(menu_item_count, 0);
    item->menu_item_callback = menu_item_callback;
    item->menu_item_user_data = user_data;
    item->menu_revision += 1;

    if (item->menu_item_count > 0) {
        item->menu_items = g_new0(SCUIStatusNotifierMenuItem, item->menu_item_count);
        for (int i = 0; i < item->menu_item_count; i++) {
            item->menu_items[i] = menu_items[i];
            item->menu_items[i].label = scui_strdup_or_empty(menu_items[i].label);
        }
    }

    if (item->connection) {
        g_dbus_connection_emit_signal(
            item->connection,
            NULL,
            "/Menu",
            "com.canonical.dbusmenu",
            "LayoutUpdated",
            g_variant_new("(ui)", item->menu_revision, 0),
            NULL
        );
    }
}

void scui_status_notifier_item_free(SCUIStatusNotifierItem *item) {
    if (!item) {
        return;
    }

    if (item->bus_owner_id != 0) {
        g_bus_unown_name(item->bus_owner_id);
    }
    if (item->connection && item->object_registration_id != 0) {
        g_dbus_connection_unregister_object(item->connection, item->object_registration_id);
    }
    if (item->connection && item->menu_registration_id != 0) {
        g_dbus_connection_unregister_object(item->connection, item->menu_registration_id);
    }
    if (item->connection) {
        g_object_unref(item->connection);
    }

    for (int i = 0; i < item->menu_item_count; i++) {
        g_free((char *)item->menu_items[i].label);
    }
    g_free(item->menu_items);

    g_free(item->id);
    g_free(item->title);
    g_free(item->icon_name);
    g_free(item->icon_path);
    g_free(item->icon_file);
    g_free(item->tooltip);
    g_free(item->bus_name);
    g_free(item);
}
