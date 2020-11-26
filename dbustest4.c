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

static void print_variant(GVariant *v, const gchar *type)
{
    switch(type[0])
    {
        case 'a':
            if(type[1] == 's')
            {
                GVariantIter iter;
                g_variant_iter_init(&iter, v);
                gchar *v2;
                while(g_variant_iter_next(&iter, "s", &v2))
                {
                    printf("%s,",v2);
                    g_free(v2);
                }
                printf("\b ");
            }
            break;
        case 's':
            {
                gchar *s;
                g_variant_get(v, "s", &s);
                printf("%s", s);
                g_free(s);
                break;
            }

    }
}

static void interface_added(GDBusConnection *connection,
                             const gchar *sender_name,
                             const gchar *object_path,
                             const gchar *interface_name,
                             const gchar *signal_name,
                             GVariant *parameters,
                             gpointer user_data)
{

    GVariantIter iter;

    g_variant_iter_init(&iter, parameters);

    gchar *new_object_path;

    g_variant_iter_next(&iter, "o", &new_object_path);
    printf("New Interface at %s\n", new_object_path);
    g_free(new_object_path);

    GVariantIter *interfaces_and_properties;
    g_variant_iter_next(&iter, "a{sa{sv}}", &interfaces_and_properties);

    printf("%d interfaces returned.\n", g_variant_iter_n_children(interfaces_and_properties));

    gchar *key1;
    GVariantIter *dict;

    while(g_variant_iter_next(interfaces_and_properties, "{sa{sv}}", &key1, &dict))
    {
        gchar *key2;
        GVariant *value;

        printf("   %s -->\n", key1);
        while(g_variant_iter_next(dict, "{sv}", &key2, &value))
        {
            printf("      %s = ", key2);
            print_variant(value, g_variant_get_type_string(value));
            puts("");
            g_free(key2);
            g_variant_unref(value);

        }
        g_free(key1);
        g_variant_iter_free(dict);

    }
    g_variant_iter_free(interfaces_and_properties);
}

int main(void)
{
    GError *error = NULL;
    GDBusConnection *dbus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &error);
    if(!dbus)
    {
        print_error("Error acquiring dbus.\n", &error);
        return 1;
    }

    guint interface_added_id = g_dbus_connection_signal_subscribe(
                                        dbus, "org.bluez", "org.freedesktop.DBus.ObjectManager",
                                        "InterfacesAdded", NULL, NULL, G_DBUS_SIGNAL_FLAGS_NONE,
                                        interface_added, NULL, NULL);
    
    GMainLoop *loop = g_main_loop_new(NULL, 0);
    g_main_loop_run(loop);

    return 0;

}
