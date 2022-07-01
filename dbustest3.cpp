#include <iostream>
#include <giomm.h>
#include <thread>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#ifdef GTK4
#define BUS_TYPE_SYSTEM SYSTEM
#endif

constexpr auto SP_UUID = "00001101-0000-1000-8000-00805f9b34fb";

static Glib::RefPtr<Gio::DBus::NodeInfo> profile_node;
static Glib::ustring Profile1_definition = 
"<node>"
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

static Glib::RefPtr<Gio::DBus::NodeInfo> agent_node;
static Glib::ustring Agent1_definition = 
"<node>"
"  <interface name='org.bluez.Agent1'>"
"     <method name='Release'>"
"     </method>"
"     <method name='RequestPinCode'>"
"        <arg type='o' name='device' direction='in'/>"
"        <arg type='s' name='pincode' direction='out'/>"
"     </method>"
"     <method name='DisplayPinCode'>"
"        <arg type='o' name='device' direction='in'/>"
"        <arg type='s' name='pincode' direction='in'/>"
"     </method>"
"     <method name='RequestPasskey'>"
"        <arg type='o' name='device' direction='in'/>"
"     </method>"
"     <method name='DisplayPasskey'>"
"        <arg type='o' name='device' direction='in'/>"
"        <arg type='u' name='passkey' direction='in'/>"
"        <arg type='q' name='entered' direction='in'/>"
"     </method>"
"     <method name='RequestConfirmation'>"
"        <arg type='o' name='device' direction='in'/>"
"        <arg type='u' name='passkey' direction='in'/>"
"     </method>"
"     <method name='RequestAuthorization'>"
"        <arg type='o' name='device' direction='in'/>"
"     </method>"
"     <method name='AuthorizeService'>"
"        <arg type='o' name='device' direction='in'/>"
"        <arg type='s' name='uuid' direction='in'/>"
"     </method>"
"     <method name='Cancel'>"
"     </method>"
"  </interface>"
"</node>";

static void socket_reader(int sock_fd)
{
    char buf[100];
    while(true)
    {
        int bytes = read(sock_fd, buf, sizeof(buf)-1);
        if(bytes >= 0)
        {
            buf[bytes] = '\0';
            std::cout << buf << std::flush;
        }
        else
            std::cout << "Oh, come on!" << std::endl;
    }
}

static void new_connection(int sock_fd)
{

    //set socket to blocking
    auto opts = fcntl(sock_fd, F_GETFL);
    if(opts < 0)
    {
        std::cout << "Error getting socket flags" << std::endl;
        return;
    }
    opts &= ~O_NONBLOCK;
    if(fcntl(sock_fd, F_SETFL, opts) < 0)
    {
        std::cout << "Error setting socket flags for nonblocking I/O" << std::endl;
        return;
    }

    std::thread reader(socket_reader, sock_fd);
    std::cin.ignore(100, '\n');
    write(sock_fd, "$$$", 3);
    std::string user_input;
    while(true)
    {
        std::getline(std::cin, user_input);
        user_input += "\r";
        write(sock_fd, user_input.c_str(), user_input.length());
    }
}

static void
agent_method(const Glib::RefPtr<Gio::DBus::Connection>&,
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
    if(method_name == "RequestPinCode")
    {
        std::string user_input;
        std::cout << "Enter Pin Code: ";
        std::cin.ignore(10, '\n');
        std::getline(std::cin, user_input);
        auto ret = Glib::VariantContainerBase::create_tuple(
                    Glib::Variant<Glib::ustring>::create(user_input));
        invocation->return_value(ret);
    }
    else
        std::cout << "Unexpected method (" << method_name << ") called...." << std::endl;
}

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
    if(method_name == "NewConnection")
    {
        auto fd_list = invocation->get_message()->get_unix_fd_list();
        std::cout << "FD List Received with " << fd_list->get_length() << " descriptors." 
                  << std::endl;

        invocation->return_value({});
        new_connection(fd_list->get(0));
    }
}

const Gio::DBus::InterfaceVTable profile_vtable(sigc::ptr_fun(&profile_method));
const Gio::DBus::InterfaceVTable agent_vtable(sigc::ptr_fun(&agent_method));

static Glib::RefPtr<Gio::DBus::Proxy> defaultAdapter;

static std::vector<Glib::RefPtr<Gio::DBus::Proxy>> device_list;

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

    for(auto it = device_list.begin(); it < device_list.end(); it++)
    {
        if(*it == Glib::RefPtr<Gio::DBus::Proxy>::cast_dynamic(obj))
            device_list.erase(it);
    }

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

void connect_complete(Glib::RefPtr<Gio::AsyncResult>& result, 
                      const Glib::RefPtr<Gio::DBus::Proxy>& device)
{
    std::cout << "connect_complete() called." << std::endl;
    try
    {
        auto status = device->call_finish(result);
        std::cout << "Connect called, " << status.get_type_string() << " returned." << std::endl;
    }
    catch(Glib::Error &e)
    {
        std::cout << e.what() << " : " << e.code() << std::endl;
    }
}

void register_agent();

void connect_device(const Glib::RefPtr<Gio::DBus::Proxy>& device)
{
    Glib::Variant<bool> paired;
    device->get_cached_property(paired, "Paired");
    
    if(!paired.get()) //Object not paired register agent with bluez to handle pairing.
        register_agent();
       
    device->call("Connect",
            sigc::bind<decltype(device)>(sigc::ptr_fun(connect_complete),
                                             device));
}

Glib::RefPtr<Gio::DBus::Proxy> bt_profile_manager;

void register_profile()
{
    std::vector<Glib::VariantBase> parameter_vector;
    auto profile = Glib::Variant<Glib::DBusObjectPathString>::create("/org/obd/serial");
    auto uuid = Glib::Variant<Glib::ustring>::create(SP_UUID);
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

Glib::RefPtr<Gio::DBus::Proxy> bt_agent_manager;

void register_agent()
{
    auto parameters = 
        Glib::VariantContainerBase::create_tuple(
            std::vector<Glib::VariantBase>(
            {
                Glib::Variant<Glib::DBusObjectPathString>::create("/org/obd/serial"),
                Glib::Variant<Glib::ustring>::create("KeyboardDisplay"),
            }));

    if(bt_agent_manager)
    {
        auto status = bt_agent_manager->call_sync("RegisterAgent", parameters);
        std::cout << "RegisterAgent() called; " << status.get_type_string() << " returned." << std::endl;
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
            if(uuid == SP_UUID)
                obd_device = device;
        }
        device_list.push_back(device);
    }  

    auto profile_manager = get_interface(obj, "org.bluez.ProfileManager1");
    if(profile_manager)
    {
        bt_profile_manager = profile_manager;
    }

    auto agent_manager = get_interface(obj, "org.bluez.AgentManager1");
    if(agent_manager)
        bt_agent_manager = agent_manager;

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

        //register_profile();
    }
    
    while(true)
    {
        std::cout << "Select Device to Connect to:" << std::endl;
        int i=1;
        for(auto &device : device_list)
        {
            Glib::Variant<Glib::ustring> address;
            Glib::Variant<Glib::ustring> name;
            device->get_cached_property(name, "Alias");
            device->get_cached_property(address, "Address");


            std::cout << i++ << ") " << name.get() << " <" << address.get() << ">" << std::endl;
        }

        std::cout << "Choose Device [1-" << device_list.size() << "]:";
        int choice;
        std::cin >> choice;
        if(choice > 0 && choice <= device_list.size())
        {
            obd_device = device_list[choice-1];
            break;
        }
    }

    if(obd_device)
        connect_device(obd_device);
    
}

void init (Glib::RefPtr<Gio::DBus::ObjectManagerClient> &manager)
{

    manager->get_connection()->register_object("/org/obd/serial", 
                                  profile_node->lookup_interface(),
                                  profile_vtable);

    manager->get_connection()->register_object("/org/obd/serial", 
                                  agent_node->lookup_interface(),
                                  agent_vtable);

    showExistingObjects(manager);

    manager->signal_object_added().connect(sigc::ptr_fun(object_added));
    manager->signal_object_removed().connect(sigc::ptr_fun(object_removed));

    register_profile();

    if(defaultAdapter)
    {
        auto status = defaultAdapter->call_sync("StartDiscovery", {}, Glib::Variant<Glib::VariantBase>());
        std::cout << "StartDiscovery called, " << status.get_type_string() << " returned." << std::endl;

    }

    Glib::signal_timeout().connect_seconds_once(sigc::ptr_fun(&stop_discovery), 10);
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
    agent_node = Gio::DBus::NodeInfo::create_for_xml(Agent1_definition);

    auto manager = Gio::DBus::ObjectManagerClient::create_for_bus_sync(Gio::DBus::BUS_TYPE_SYSTEM,
                   "org.bluez","/");
    init(manager);

    auto loop = Glib::MainLoop::create();


    loop->run();
}
