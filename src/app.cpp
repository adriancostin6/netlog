#include "nped/app.h"

#include <stdexcept>
#include <string>
#include <thread>
#include <iostream>
//#include <chrono>

#include "nped/sniffer.h"

namespace nped {

Application::Application(int argc, char *argv[]) : arg_parser_{argc, argv} { }

void Application::start() { 
    try {
        // Search for interface and filters
        const auto& interface = arg_parser_.find_flag("i");
        const auto& pcap_filter = arg_parser_.find_flag("f");

        if (!interface) {
            throw std::invalid_argument("Invalid input arguments");
        }

        const auto& iface_value = interface.value()[0];

        const SnifferType st = 
            arg_parser_.find_switch("l") || arg_parser_.find_switch("live") ? 
            SnifferType::Sniffer : SnifferType::FileSniffer;

        // Populate filters 
        const std::string pcap_filter_value = [&pcap_filter] {
            std::string f;
            if (pcap_filter) {
                for (auto& s: pcap_filter.value()) {
                    f.append(s);
                    f.append(" ");
                }
                f.pop_back();
            }
            return f;
        }();

        bool running = true;

        // TODO start thread for keypress detection to stop capture -- nested lambda?
        std::thread producer([&pq = packetq_, st, &running,
                             &iface_value, &pcap_filter_value]() {
                                 PacketSniffer ps(st, iface_value.data(),
                                                  pcap_filter_value.data());
                                 ps.run(pq, running);
                             });


        std::thread consumer([&pq = packetq_, &edp = edited_packets_, &running]() {
                                 for (;;) {
                                     if (!running) {
                                         break;
                                     }

                                     if (pq.empty()) {
                                         continue;
                                     } else {
                                         auto packet = pq.pop();
                                         edp.push_back(packet);
                                     }
                                 } 
                             });

        std::thread wait_for_key([&running, &producer, &consumer]() {
                                     std::cout << "Press key to stop capture\n";
                                     std::cin.get();

                                     running = false;
                                     producer.join();
                                     consumer.join();
                                 });

        wait_for_key.join();

        // TODO start a second thread for live capture

        //        PacketSniffer ps(st, iface_value.data(), pcap_filter_value.data());

        // TODO thread 1 - capture packets -- accesses packet pool
        //       ps.run(packetq_);

        // TODO thread 2 - edit packets -- accesses packet pool
        //        PacketEditor pe;
        //        pe.edit_next_packet(PacketPool& p);
        std::cout << "Work is done! Captured and edited "<< 
            edited_packets_.size() << " packets.\n" <<
            "Press any key to exit...";
        std::cin.get();
    } catch (std::exception &e) {
        throw std::runtime_error(e.what());
    }
}

}