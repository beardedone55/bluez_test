#include <iostream>
#include <giomm.h>
#include <stdio.h>

int main()
{
    Gio::init();
    auto manager = Gio::DBus::ObjectManagerClient::create_for_bus_sync(Gio::DBus::BUS_TYPE_SYSTEM,
                   "org.bluez","/");
    if(!manager)
        std::cout << "ERROR!!!" << std::endl;


    std::cout << "Manager name: " << manager->get_name() << std::endl;


    auto objects = manager->get_objects();

    for(auto object : objects)
    {
        auto object_path = object->get_object_path();

        std::cout << "[ " << object_path << " : " << object.get() << " ]" << std::endl;

        auto adapter_interface = manager->get_interface(object_path, "org.bluez.Adapter1");

        std::cout << "   adapter interface: " << adapter_interface.get() << std::endl;
    }

    auto loop = Glib::MainLoop::create();
    loop->run();

    return 0;
}
