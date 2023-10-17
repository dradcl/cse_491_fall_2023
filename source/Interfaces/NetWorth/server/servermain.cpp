/**
 * This file is part of the Fall 2023, CSE 491 course project.
 * @brief A simplistic main file to demonstrate a system.
 * @note Status: PROPOSAL
 **/

// Include the modules that we will be using.
#include <sstream>
#include<string>
#include<vector>
#include "../../../core/Data.hpp"
#include "../../TrashInterface.hpp"
#include "../../../Agents/PacingAgent.hpp"
#include "../../../Worlds/MazeWorld.hpp"
#include "../NetworkInterface.hpp"
#include "networkingworld.hpp"
#include "ServerPlayerInterface.hpp"

Packet GridToPacket(const cse491::WorldGrid & grid, const cse491::type_options_t & type_options,
                  const cse491::item_set_t & item_set, const cse491::agent_set_t & agent_set)
    {
    std::vector<std::string> packet_grid(grid.GetHeight());

    // Load the world into the symbol_grid;
    for (size_t y=0; y < grid.GetHeight(); ++y) {
    packet_grid[y].resize(grid.GetWidth());
        for (size_t x=0; x < grid.GetWidth(); ++x) {
            packet_grid[y][x] = type_options[ grid.At(x,y) ].symbol;
        }
    }

    // Add in the agents / entities
    for (const auto & entity_ptr : item_set) {
    cse491::GridPosition pos = entity_ptr->GetPosition();
    packet_grid[pos.CellY()][pos.CellX()] = '+';
    }

    for (const auto & agent_ptr : agent_set) {
    cse491::GridPosition pos = agent_ptr->GetPosition();
    char c = '*';
    if(agent_ptr->HasProperty("char")){
        c = static_cast<char>(agent_ptr->GetProperty("char"));
    }
    packet_grid[pos.CellY()][pos.CellX()] = c;
    }


    // Print out the symbol_grid with a box around it.
    std::ostringstream oss;
    oss << '+' << std::string(grid.GetWidth(),'-') << "+\n";
    for (const auto & row : packet_grid) {
    oss << "|";
    for (char cell : row) {
        // std::cout << ' ' << cell;
        oss << cell;
    }
    oss << "|\n";
    }
    oss << '+' << std::string(grid.GetWidth(),'-') << "+\n";
    oss << "\nYour move? ";
    std::string gridString = oss.str();

    Packet gridPacket;
    gridPacket << gridString;

    return gridPacket;
}

int main()
{
    //Create the world on the server side upon initialization
    //Add the pacing agents to just simply walk around
    //Once everything is added we simply wait for a response from a client that is connecting.
<<<<<<< Updated upstream
    cse491::netWorth::NetworkMazeWorld world;
    world.AddAgent<cse491::PacingAgent>("Pacer 1").SetPosition(3,1);
    world.AddAgent<cse491::PacingAgent>("Pacer 2").SetPosition(6,1);
    world.AddAgent<cse491::TrashInterface>("Interface").SetProperty("char", '@');
=======
    std::shared_ptr<netWorth::NetworkMazeWorld> world = std::make_shared<netWorth::NetworkMazeWorld>();
    world->AddAgent<cse491::PacingAgent>("Pacer 1").SetPosition(3,1);
    world->AddAgent<cse491::PacingAgent>("Pacer 2").SetPosition(6,1);
    world->AddAgent<NetWorth::ServerPlayerInterface>("Interface").SetProperty("symbol", '@');
    world->SetPlayerAgent();
>>>>>>> Stashed changes

    UdpSocket serverSocket;
    serverSocket.bind(55002);

    std::cout << sf::IpAddress::getLocalAddress() << std::endl;

    // Receive a message from anyone
    sf::Packet send_pkt;
    sf::Packet recv_pkt;
    std::string str;

    sf::IpAddress sender;
    unsigned short port;
    serverSocket.receive(recv_pkt, sender, port);
    recv_pkt >> str;
    std::cout << sender.toString() << " said: " << str << std::endl;
    
    if(!str.empty()){
        std::string msg = "Pong!";
        send_pkt << msg;
        serverSocket.send(send_pkt, sender, port);
        //world.NetworkRun(&serverSocket, sender, port);
        cse491::item_set_t item_set;
        cse491::agent_set_t agent_set;
        std::string input;

        while (true) {
            sf::Packet gridPacket = GridToPacket(world.GetGrid(), world.GetCellTypes(), item_set, agent_set);
            serverSocket.send(gridPacket, sender, port);

            serverSocket.receive(recv_pkt, sender, port);
            recv_pkt >> input;
            std::cout << input << std::endl;

            if (input == "quit") break;

            /*
            if (input == 'q') {
                message = "quit";
                serverSocket.send(message.c_str(), message.size() + 1, sender, port);
                break;
            }
            */

        }

<<<<<<< Updated upstream
=======
        if (serverSocket->receive(recv_pkt, sender, port) != sf::Socket::Status::Done) {
            std::cout << "Failure to receive" << std::endl;
            return 1;
        }

        recv_pkt >> input;
        std::cout << input << std::endl;

        if (input == "quit") break;

        world->SetPlayerAction(input);
        world->RunAgents();
>>>>>>> Stashed changes
    }
    
}