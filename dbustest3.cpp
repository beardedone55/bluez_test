#include <iostream>
#include <giomm.h>

#ifdef GTK4
#define BUS_TYPE_SYSTEM SYSTEM
#endif

static Glib::RefPtr<Gio::DBus::NodeInfo> profile_node;
static Glib::ustring Profile1_definition = "<node>"
                                           "  <interface name='org.bluez.Profile1'>"
                                           "    <method name='Release'>"
                                           "    </method>"
                                           "    <method name='NewConnection'>"
                                           "      <arg type='o' name='device' direction='in'/>"
                                           "      <arg type='h' name='fd' direction='in'/>"
                                           "      <arg type='a{sv}' name='fd_properties' direction='in'/>"
                                           "    </method>"
                                           "    <method name='RequestDisconnection'>"
                                           "      <arg type='o' name='device' direction='in'/>"
                                           "    </method>"
                                           "  </interface>"
                                           "</node>";

static void
profile_method(const Glib::RefPtr<Gio::DBus::Connection>&,
               const Glib::ustring & sender,
               const Glib::ustring & object_path,
               const Glib::ustring & interface_name,
               const Glib::ustring & method_name,
               const Glib::VariantContainerBase& parameters,
               const Glib::RefPtr<Gio::DBus::MethodInvocation>& invocation)
{
    std::cout << method_name << " for " << interface_name << " called." << std::endl;
    std::cout << "Object Path = " << object_path << std::endl;
    std::cout << "Parameters = " << parameters.print() << std::endl;
}

const Gio::DBus::InterfaceVTable profile_vtable(sigc::ptr_fun(&profile_method));

static Glib::RefPtr<Gio::DBus::Proxy> defaultAdapter;

static void interface_handler(gpointer interface, gpointer not_used)
{
    const gchar *name = g_dbus_proxy_get_interface_name((GDBusProxy *)interface);
    std::cout << "   " << name << std::endl; 
}

Glib::RefPtr<Gio::DBus::Proxy> get_interface(const Glib::RefPtr<Gio::DBus::Object>&obj,
                                             Glib::ustring name)
{
#ifdef GTK4
    return std::dynamic_pointer_cast<Gio::DBus::Proxy>(obj->get_interface(name));
#else
    return Glib::wrap((GDBusProxy *)g_dbus_object_get_interface(obj->gobj(), name.c_str()));
#endif
}

void object_removed(const Glib::RefPtr<Gio::DBus::Object>& obj)
{
    std::cout << std::endl << obj->get_object_path() << " removed." << std::endl;
    auto interface = get_interface(obj, "org.bluez.Adapter1");
    if(defaultAdapter == interface)
        defaultAdapter.reset();

    //std::cout << "defaultAdapter = " << defaultAdapter.get() << std::endl;
}

static void print_interfaces(const Glib::RefPtr<Gio::DBus::Object>& obj)
{
#ifdef GTK4
    auto interfaces = obj->get_interfaces();
    for(const auto &it : interfaces)
    {
        auto interface = std::dynamic_pointer_cast<Gio::DBus::Proxy>(it);
        if(interface)
            std::cout << "   " << interface->get_interface_name() << std::endl;
    }
#else
    auto interfaces = g_dbus_object_get_interfaces(obj->gobj());
    g_list_foreach(interfaces, interface_handler, nullptr);
#endif    
}

void connect_device(const Glib::RefPtr<Gio::DBus::Proxy>& device)
{
    auto status = device->call_sync("Connect", {}, Glib::Variant<Glib::VariantBase>());
    std::cout << "Connect called, " << status.get_type_string() << " returned." << std::endl;

}

Glib::RefPtr<Gio::DBus::Proxy> bt_profile_manager;

void register_profile()
{
    std::vector<Glib::VariantBase> parameter_vector;
    auto profile = Glib::Variant<Glib::DBusObjectPathString>::create("/org/obd/serial");
    auto uuid = Glib::Variant<Glib::ustring>::create("00001101-0000-1000-8000-00805f9b34fb");
    parameter_vector.push_back(profile);
    parameter_vector.push_back(uuid);

    std::map<Glib::ustring, Glib::VariantBase> options;
    Glib::VariantBase name = Glib::Variant<Glib::ustring>::create("obd-serial");
    options["Name"] = Glib::Variant<Glib::ustring>::create("obd-serial");
    options["Service"] = uuid;
    options["Role"] = Glib::Variant<Glib::ustring>::create("client");
    options["Channel"] = Glib::Variant<guint16>::create(1);
    options["AutoConnect"] = Glib::Variant<bool>::create(true);

    parameter_vector.push_back(Glib::Variant<std::map<Glib::ustring, Glib::VariantBase>>::create(options));

    auto parameters = Glib::VariantContainerBase::create_tuple(parameter_vector);

    std::cout << "Register Profile parameters = " << parameters.get_type_string() << std::endl;

    std::cout << parameters.print() << std::endl;

    if(bt_profile_manager)
    {
        auto status = bt_profile_manager->call_sync("RegisterProfile", parameters);
        std::cout << "RegisterProfile() called; " << status.get_type_string() << " returned." << std::endl;
    }
}

Glib::RefPtr<Gio::DBus::Proxy> obd_device;

void object_added(const Glib::RefPtr<Gio::DBus::Object>& obj)
{
    std::cout << std::endl << "[ " << obj->get_object_path() << " ]" << std::endl;

    print_interfaces(obj);

    auto adapter = get_interface(obj, "org.bluez.Adapter1");
    if(adapter && !defaultAdapter)
        defaultAdapter = adapter;

    auto device = get_interface(obj, "org.bluez.Device1");

    if(device)
    {
        Glib::Variant<Glib::ustring> address;
        Glib::Variant<Glib::ustring> name;
        Glib::Variant<std::vector<Glib::ustring>> UUIDs;
        Glib::Variant<bool> connected;
        Glib::Variant<bool> paired;

        device->get_cached_property(address, "Address");
        device->get_cached_property(name, "Alias");
        device->get_cached_property(UUIDs, "UUIDs");
        device->get_cached_property(connected, "Connected");
        device->get_cached_property(paired, "Paired");

        std::cout << "    Device Address = " << address.get() << std::endl;
        std::cout << "    Device Name = " << name.get() << std::endl;
        std::cout << "    Device connected = " << connected.get() << std::endl;
        std::cout << "    Device paired = " << paired.get() << std::endl;
        std::cout << "    UUIDs type = " << UUIDs.get_type_string() << std::endl;
        for( auto &uuid : UUIDs.get())
        {
            std::cout << "    Service UUID = " << uuid << std::endl;
        }

        if(address.get() == "00:06:66:18:B4:54")
            obd_device = device;
    }  

    auto profile_manager = get_interface(obj, "org.bluez.ProfileManager1");
    if(profile_manager)
    {
        bt_profile_manager = profile_manager;
    }

    //std::cout << "defaultAdapter = " << defaultAdapter.get() << std::endl;

}

void showExistingObjects(Glib::RefPtr<Gio::DBus::ObjectManagerClient> &manager)
{
    auto objects = manager->get_objects();
    for(auto &object : objects)
        object_added(object);
}

void stop_discovery()
{
    if(defaultAdapter)
    {
        auto status = defaultAdapter->call_sync("StopDiscovery", {}, 
                                                 Glib::Variant<Glib::VariantBase>());
        std::cout << "StopDiscovery called, " << status.get_type_string() << 
                          " returned." << std::endl;

        //if(obd_device)
        //    connect_device(obd_device);
        //register_profile();
    }
    
}

void init (Glib::RefPtr<Gio::DBus::ObjectManagerClient> &manager)
{

    manager->get_connection()->register_object("/org/obd/serial", 
                                  profile_node->lookup_interface(),
                                  profile_vtable);

    showExistingObjects(manager);

    manager->signal_object_added().connect(sigc::ptr_fun(object_added));
    manager->signal_object_removed().connect(sigc::ptr_fun(object_removed));

    register_profile();

    if(defaultAdapter)
    {
        auto status = defaultAdapter->call_sync("StartDiscovery", {}, Glib::Variant<Glib::VariantBase>());
        std::cout << "StartDiscovery called, " << status.get_type_string() << " returned." << std::endl;

    }

    Glib::signal_timeout().connect_seconds_once(sigc::ptr_fun(&stop_discovery), 30);
}

void
name_acquired(const Glib::RefPtr<Gio::DBus::Connection> & connection,
             const Glib::ustring & name)
{
    std::cout << "Name " << name << " acquired..." << std::endl;
}

void
name_lost(const Glib::RefPtr<Gio::DBus::Connection> & connection,
             const Glib::ustring & name)
{
    std::cout << "Name " << name << " not acquired..." << std::endl;
}

int main()
{
    Gio::init();
    profile_node = Gio::DBus::NodeInfo::create_for_xml(Profile1_definition);

    auto manager = Gio::DBus::ObjectManagerClient::create_for_bus_sync(Gio::DBus::BUS_TYPE_SYSTEM,
                   "org.bluez","/");
    init(manager);

    auto loop = Glib::MainLoop::create();


    loop->run();
}
