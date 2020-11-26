#include <gio/gio.h>
#include <stdio.h>

static inline void print_error(char *message, GError **error)
{
    GError *err = *error;
    puts(message);
    printf("  %s\n", err->message);
    g_error_free(err);
    *error = NULL;
}

static void interface_handler(gpointer interface, gpointer not_used)
{
    const gchar *name = g_dbus_proxy_get_interface_name(interface);
    printf("   %s\n", name); 
}

static void object_handler(gpointer object, gpointer not_used)
{
    printf("[ %s ]\n", g_dbus_object_get_object_path(object));

    GList *interfaces = g_dbus_object_get_interfaces(object);
    if(interfaces)
    {
        g_list_foreach(interfaces, interface_handler, NULL);
        g_list_free_full(interfaces, g_object_unref);
    }
}

int main(void)
{
    GError *error = NULL;
    GDBusObjectManager *manager = g_dbus_object_manager_client_new_for_bus_sync(
                G_BUS_TYPE_SYSTEM,
                G_DBUS_OBJECT_MANAGER_CLIENT_FLAGS_NONE,
                "org.bluez",
                "/",
                NULL,
                NULL,
                NULL,
                NULL,
                &error
            );

    if(!manager)
    {
        print_error("Error while acquiring object manager\n", &error);
        return 1;
    }

    printf("Manager name: %s\n", g_dbus_object_manager_client_get_name(
                 G_DBUS_OBJECT_MANAGER_CLIENT(manager)));

    GList *objects = g_dbus_object_manager_get_objects(manager);
    if(objects)
    {
        g_list_foreach(objects, object_handler, NULL);
        g_list_free_full(objects, g_object_unref);
    }

    g_object_unref(manager);
    return 0;
}
