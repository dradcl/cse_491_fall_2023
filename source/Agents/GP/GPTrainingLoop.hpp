
#pragma once

#include "../../core/AgentBase.hpp"
#include "../../core/WorldBase.hpp"
//#include "GPAgent.hpp"
#include "GPAgent_.hpp"


#include "CGPAgent.hpp"

#include <thread>
#include <iostream>
#include <vector>
#include <ranges>
#include <cmath>
#include <filesystem>
#include <string>
#include <sstream>
#include <chrono>


#include "tinyxml2.h"
//#include <algorithm>


namespace cowboys {

    template<class AgentType, class EnvironmentType>
    class GPTrainingLoop {
    private:

        std::vector<cse491::WorldBase *> environments;
        std::vector<std::vector<cowboys::GPAgent_ *>> agents;


        std::vector<std::vector<cse491::GridPosition>> TEMPinitialAgentPositions;
//        std::vector<std::vector<std::vector<cse491::GridPosition>>> TEMPinitialAgentPositions;


        std::vector<std::vector<double>> TEMPAgentFitness;

        tinyxml2::XMLDocument doc;
        tinyxml2::XMLDocument lastGenerationsDoc;


        tinyxml2::XMLElement *root = doc.NewElement("GPLoop");


        tinyxml2::XMLElement *rootTEMP = lastGenerationsDoc.NewElement("GPLoop");

        std::vector<std::pair<int, int>> sortedAgents = std::vector<std::pair<int, int>>();



    public:

        GPTrainingLoop() {

          doc.InsertFirstChild(root);
          resetMainTagLastGenerations();

        }

        void resetMainTagLastGenerations() {
          rootTEMP = lastGenerationsDoc.NewElement("GPLoop");
          lastGenerationsDoc.InsertFirstChild(rootTEMP);
        }

        /**
         * @brief Initialize the training loop with a number of environments and agents per environment.
         * @param numArenas
         * @param NumAgentsForArena
         */
        void initialize(size_t numArenas = 5, size_t NumAgentsForArena = 100) {


          for (size_t i = 0; i < numArenas; ++i) {
            // instantiate a new environment
            environments.emplace_back(new EnvironmentType());

            agents.emplace_back(std::vector<cowboys::GPAgent_ *>());
            TEMPinitialAgentPositions.emplace_back(std::vector<cse491::GridPosition>());
            for (size_t j = 0; j < NumAgentsForArena; ++j) {

              cowboys::GPAgent_ &addedAgent = static_cast<cowboys::GPAgent_ &>(environments[i]->template AddAgent<AgentType>(
                      "Agent " + std::to_string(j)));
              addedAgent.SetPosition(0, 0);
              cse491::GridPosition position = addedAgent.GetPosition();

              TEMPinitialAgentPositions[i].emplace_back(position);

              agents[i].emplace_back(&addedAgent);

            }

          }

        }


        double simpleFitnessFunction(cse491::AgentBase &agent, cse491::GridPosition startPosition) {

          cse491::GridPosition currentPosition = agent.GetPosition();

          // Eucledian distance
          double distance = std::sqrt(std::pow(currentPosition.GetX() - startPosition.GetX(), 2) +
                                      std::pow(currentPosition.GetY() - startPosition.GetY(), 2));

          // Agent complexity, temporarily doing this in a bad way
          double complexity = 0;
          if (auto *cgp = dynamic_cast<CGPAgent *>(&agent)) {
            auto genotype = cgp->GetGenotype();
            complexity = std::divides<double>()(genotype.GetNumConnections(), genotype.GetNumPossibleConnections());
          }

          return distance - complexity;
        }


        void run(size_t numGenerations,
                 size_t numberOfTurns = 100,
                 size_t maxThreads = 0) {

          /// XML save filename data
          auto now = std::chrono::system_clock::now();
          std::time_t now_time = std::chrono::system_clock::to_time_t(now);

          // Format the date and time as a string (hour-minute-second)
          std::tm tm_time = *std::localtime(&now_time);
          std::ostringstream oss;
          oss << std::put_time(&tm_time, "%Y-%m-%d__%H_%M_%S");
          std::string dateTimeStr = oss.str();


          std::string relativePath = "../../savedata/GPAgent/";
          std::filesystem::path absolutePath = std::filesystem::absolute(relativePath);
          std::filesystem::path normalizedAbsolutePath = std::filesystem::canonical(absolutePath);

          const std::string filename = "AgentData_" + dateTimeStr + ".xml";
          auto fullPath = normalizedAbsolutePath / filename;

          const std::string lastGenerationsFilename = "lastGenerations_" + dateTimeStr + ".xml";
          auto lastGenerationsFullPath = normalizedAbsolutePath / lastGenerationsFilename;


          for (size_t generation = 0; generation < numGenerations; ++generation) {

            std::vector<std::thread> threads;

            for (size_t arena = 0; arena < environments.size(); ++arena) {
              if (maxThreads == 0 || threads.size() < maxThreads) {
                threads.emplace_back(&GPTrainingLoop::runArena, this, arena, numberOfTurns);
              } else {
                // Wait for one of the existing threads to finish
                threads[0].join();
                threads.erase(threads.begin());
                threads.emplace_back(&GPTrainingLoop::runArena, this, arena, numberOfTurns);
              }
            }

            // Wait for all threads to finish
            for (auto &thread: threads) {
              thread.join();
              //progress for the threads
              std::cout << ".";

            }


            std::cout.flush();



            ComputeFitness();

            sortedAgents.clear();
            sortThemAgents();

            int countMaxAgents = AgentAnalysisComputations(generation);
            if (generation % 10 == 0) {

              saveEverySoOften(fullPath.string(), lastGenerationsFullPath.string());
              lastGenerationsDoc.Clear();
              resetMainTagLastGenerations();

              std::cout << "@@@@@@@@@@@@@@@@@@@@@@  " << "DataSaved" << "  @@@@@@@@@@@@@@@@@@@@@@" << std::endl;

            }


            serializeAgents(countMaxAgents, generation);

            GpLoopMutateHelper();


            resetEnvironments();


          }


          saveEverySoOften(fullPath.string(), lastGenerationsFullPath.string());
          std::cout << "@@@@@@@@@@@@@@@@@@@@@@  " << "DataSaved" << "  @@@@@@@@@@@@@@@@@@@@@@" << std::endl;


        }


        int AgentAnalysisComputations(int generation){
          // print average fitness
          double averageFitness = 0;
          double maxFitness = 0;

          std::pair<int, int> bestAgent = std::make_pair(-1, -1);
          int countMaxAgents = 0;
          for (size_t arena = 0; arena < environments.size(); ++arena) {
            for (size_t a = 0; a < agents[arena].size(); ++a) {
              averageFitness += TEMPAgentFitness[arena][a];
              if (TEMPAgentFitness[arena][a] > maxFitness) {
                maxFitness = TEMPAgentFitness[arena][a];
                bestAgent = std::make_pair(arena, a);
                countMaxAgents = 1;
              }

              if (abs(TEMPAgentFitness[arena][a] - maxFitness) < 0.001) {
                countMaxAgents++;
              }
            }
          }

          averageFitness /= (environments.size() * agents[0].size());

          std::cout << "Generation " << generation << " complete" << std::endl;
          std::cout << "Average fitness: " << averageFitness << " ";
          std::cout << "Max fitness: " << maxFitness << std::endl;
          std::cout << "Best agent: AGENT[" << bestAgent.first << "," << bestAgent.second << "] " << std::endl;
          cse491::GridPosition bestAgentPosition = agents[bestAgent.first][bestAgent.second]->GetPosition();
          std::cout << "Best Agent Final Position: " << bestAgentPosition.GetX() << "," << bestAgentPosition.GetY()
                    << std::endl;
          std::cout << "Number of agents with max fitness: " << countMaxAgents << std::endl;
          std::cout << "------------------------------------------------------------------" << std::endl;
          return countMaxAgents;
        }


        void sortThemAgents(){
          for (size_t arena = 0; arena < environments.size(); ++arena) {
            for (size_t a = 0; a < agents[arena].size(); ++a) {
              sortedAgents.push_back(std::make_pair(arena, a));
            }
          }

          std::sort(sortedAgents.begin(), sortedAgents.end(),
                    [&](const std::pair<int, int> &a, const std::pair<int, int> &b) {
                        return TEMPAgentFitness[a.first][a.second] > TEMPAgentFitness[b.first][b.second];
          });
        }

        /**
         *
         * @param fullPath
         * @param lastGenerationsFullPath
         */
        void saveEverySoOften(std::string fullPath, std::string lastGenerationsFullPath) {


          if (doc.SaveFile(fullPath.c_str()) == tinyxml2::XML_SUCCESS) {
            // std::filesystem::path fullPath = std::filesystem::absolute("example.xml");
            std::cout << "XML file saved successfully to: " << fullPath << std::endl;
          } else {
            std::cout << "Error saving XML file." << std::endl;
          }

          if (lastGenerationsDoc.SaveFile(lastGenerationsFullPath.c_str()) == tinyxml2::XML_SUCCESS) {
            std::cout << "XML file saved successfully to: " << "lastGenerations.xml" << std::endl;
          } else {
            std::cout << "Error saving XML file." << std::endl;
          }

        }


        void serializeAgents(int countMaxAgents, int generation, size_t topN = 5) {

          const char *tagName = ("generation_" + std::to_string(generation)).c_str();
          auto *generationTag = doc.NewElement(tagName);
          auto *lastGenerationsRoot = lastGenerationsDoc.NewElement(tagName);


          root->InsertFirstChild(generationTag);
          rootTEMP->InsertFirstChild(lastGenerationsRoot);

          for (int i = 0; i < std::min(sortedAgents.size(), topN); ++i) {
            auto [arenaIDX, agentIDX] = sortedAgents[i];
            agents[arenaIDX][agentIDX]->serialize(doc, generationTag, TEMPAgentFitness[arenaIDX][agentIDX]);

            agents[arenaIDX][agentIDX]->serialize(lastGenerationsDoc, lastGenerationsRoot,
                                                  TEMPAgentFitness[arenaIDX][agentIDX]);
          }


        }


        void ComputeFitness() {
          for (size_t arena = 0; arena < environments.size(); ++arena) {
            TEMPAgentFitness.emplace_back(std::vector<double>());
            for (size_t a = 0; a < agents[arena].size(); ++a) {
              double fitness = simpleFitnessFunction(*agents[arena][a], TEMPinitialAgentPositions[arena][a]);
              TEMPAgentFitness[arena].push_back(fitness);
            }
          }
        }


        void GpLoopMutateHelper() {

          constexpr double ELITE_POPULATION_PERCENT = 0.05;
          constexpr double UNFIT_POPULATION_PERCENT = 0.1;


          std::vector<std::pair<int, int>> sortedAgents = std::vector<std::pair<int, int>>();

          for (size_t arena = 0; arena < environments.size(); ++arena) {
            for (size_t a = 0; a < agents[arena].size(); ++a) {
              sortedAgents.push_back(std::make_pair(arena, a));
            }
          }
          const int ELITE_POPULATION_SIZE = int(ELITE_POPULATION_PERCENT * sortedAgents.size());


          double averageEliteFitness = 0;
          for (int i = 0; i < ELITE_POPULATION_SIZE; i++) {
            auto [arenaIDX, agentIDX] = sortedAgents[i];
            averageEliteFitness += TEMPAgentFitness[arenaIDX][agentIDX];
          }

          // std::cout << " --- average elite percent " << averageEliteFitness << "------ " << std::endl;


          std::sort(sortedAgents.begin(), sortedAgents.end(),
                    [&](const std::pair<int, int> &a, const std::pair<int, int> &b) {
                        return TEMPAgentFitness[a.first][a.second] > TEMPAgentFitness[b.first][b.second];
                    });


//      for (int i = 0; i < ELITE_POPULATION_SIZE; i++){
//        auto [arenaIDX, agentIDX] = sortedAgents[i];
//        std::cout << "-" << i <<  "elite:" << "[" << TEMPAgentFitness[arenaIDX][agentIDX] <<"]" ;
//      }

          const int MIDDLE_MUTATE_ENDBOUND = int(sortedAgents.size() * (1 - UNFIT_POPULATION_PERCENT));
          const int MIDDLE_MUTATE_STARTBOUND = int(ELITE_POPULATION_PERCENT * sortedAgents.size());


          for (int i = MIDDLE_MUTATE_STARTBOUND; i < MIDDLE_MUTATE_ENDBOUND; i++) {
            auto [arenaIDX, agentIDX] = sortedAgents[i];
            agents[arenaIDX][agentIDX]->MutateAgent(0.05);

            // if (i % (sortedAgents.size() / 10) == 0) {
            //   std::cout << " --- mutation " << " complete " << (i * 1.0 / sortedAgents.size()) << std::endl;
            // }
          }

          int unfitAgents = int(sortedAgents.size() * UNFIT_POPULATION_PERCENT);
          for (size_t i = MIDDLE_MUTATE_ENDBOUND; i < sortedAgents.size(); i++) {
            auto [arenaIDX, agentIDX] = sortedAgents[i];
            auto eliteINDEX = rand() % ELITE_POPULATION_SIZE;


            auto [eliteArenaIDX, eliteAgentIDX] = sortedAgents[eliteINDEX];
            agents[arenaIDX][agentIDX]->Copy(*agents[eliteArenaIDX][eliteAgentIDX]);

            agents[arenaIDX][agentIDX]->MutateAgent(0.01);
          }
//      printGrids();

          // std::cout << " --- mutation complete --- " << std::endl;
        }


        void printgrid(int arena) {
          auto &grid = environments[arena]->GetGrid();
          std::vector<std::string> symbol_grid(grid.GetHeight());


          const auto &type_options = environments[arena]->GetCellTypes();
          // Load the world into the symbol_grid;
          for (size_t y = 0; y < grid.GetHeight(); ++y) {
            symbol_grid[y].resize(grid.GetWidth());
            for (size_t x = 0; x < grid.GetWidth(); ++x) {
              symbol_grid[y][x] = type_options[grid.At(x, y)].symbol;
            }
          }


//      // Add in the agents / entities
//      for (const auto & entity_ptr : item_set) {
//        cse491::GridPosition pos = entity_ptr->GetPosition();
//        symbol_grid[pos.CellY()][pos.CellX()] = '+';
//      }


          const auto &agent_set = agents[arena];
          for (const auto &agent_ptr: agent_set) {
            cse491::GridPosition pos = agent_ptr->GetPosition();
            char c = '*';
            if (agent_ptr->HasProperty("symbol")) {
              c = agent_ptr->template GetProperty<char>("symbol");
            }
            symbol_grid[pos.CellY()][pos.CellX()] = c;
          }

          // Print out the symbol_grid with a box around it.
          std::cout << '+' << std::string(grid.GetWidth(), '-') << "+\n";
          for (const auto &row: symbol_grid) {
            std::cout << "|";
            for (char cell: row) {
              // std::cout << ' ' << cell;
              std::cout << cell;
            }
            std::cout << "|\n";
          }
        }


        void printGrids() {
          for (size_t arena = 0; arena < environments.size(); ++arena) {
            std::cout << "Arena " << arena << std::endl;
            printgrid(arena);
          }
        }


        void resetEnvironments() {

          for (size_t arena = 0; arena < environments.size(); ++arena) {
            for (size_t a = 0; a < agents[arena].size(); ++a) {
              agents[arena][a]->SetPosition(TEMPinitialAgentPositions[arena][a]);
              agents[arena][a]->reset();
            }
          }

          TEMPAgentFitness.clear();
        }

        void runArena(size_t arena, size_t numberOfTurns) {
          for (size_t turn = 0; turn < numberOfTurns; turn++) {
            environments[arena]->RunAgents();
            environments[arena]->UpdateWorld();
          }
        }

        ~GPTrainingLoop() = default;

    };
}