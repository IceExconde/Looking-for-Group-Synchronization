#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>
#include <limits>
#include <string>
#include <cctype>

using namespace std;

// Function to get validated integer input.
// It reads a full line and ensures that the input is a valid whole number (only digits allowed).
// If mustBePositive is true, the input must be > 0; otherwise, it must be >= 0.
int getValidInput(const string& prompt, bool mustBePositive) {
    while (true) {
        cout << prompt;
        string input;
        getline(cin, input);

        // Remove leading/trailing spaces.
        size_t start = input.find_first_not_of(" \t");
        size_t end = input.find_last_not_of(" \t");
        if (start == string::npos) {
            cout << "Input cannot be empty." << endl;
            continue;
        }
        input = input.substr(start, end - start + 1);

        // Check that every character is a digit.
        bool valid = true;
        for (char c : input) {
            if (!isdigit(c)) {
                valid = false;
                break;
            }
        }
        if (!valid) {
            cout << "Invalid input. Please enter a valid whole number (no signs or decimals allowed)." << endl;
            continue;
        }

        // Convert string to integer.
        try {
            int value = stoi(input);
            if (mustBePositive && value <= 0) {
                cout << "Input must be a positive integer." << endl;
                continue;
            }
            if (!mustBePositive && value < 0) {
                cout << "Input must be a non-negative integer." << endl;
                continue;
            }
            return value;
        }
        catch (const exception& e) {
            cout << "Invalid input. Please enter a valid whole number." << endl;
            continue;
        }
    }
}

// Structure to keep track of each instance's state and statistics.
struct Instance {
    int id;
    bool active;
    int partiesServed;
    int totalTimeServed; // in seconds
};

// Global shared data protected by a mutex.
mutex mtx;
condition_variable cv;
// Queue holding the IDs of currently available instances.
queue<int> availableInstances;
// Vector holding all instance information.
vector<Instance> instances;

// Function to simulate a party running a dungeon instance.
// Each party thread waits until an instance is available,
// then "runs" the dungeon (by sleeping a random time) and updates stats.
void partyFunction(int partyId, int t1, int t2) {
    int instanceId;
    {
        unique_lock<mutex> lock(mtx);
        // Wait until at least one instance is available.
        cv.wait(lock, [] { return !availableInstances.empty(); });
        instanceId = availableInstances.front();
        availableInstances.pop();
        instances[instanceId].active = true;
        cout << "Party " << partyId << " entered instance " << instanceId << ".\n";
    }

    // Create a random duration between t1 and t2 seconds.
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(t1, t2);
    int runTime = dis(gen);

    // Simulate dungeon run.
    this_thread::sleep_for(chrono::seconds(runTime));

    {
        unique_lock<mutex> lock(mtx);
        // Update instance stats.
        instances[instanceId].active = false;
        instances[instanceId].partiesServed++;
        instances[instanceId].totalTimeServed += runTime;
        cout << "Party " << partyId << " finished instance " << instanceId
            << " in " << runTime << " seconds.\n";
        // Return the instance id to the available queue.
        availableInstances.push(instanceId);
    }
    cv.notify_all(); // Notify waiting threads that an instance is now available.
}

int main() {
    // Input error handling.
    int n = getValidInput("Enter maximum number of concurrent instances (n): ", true);
    int tanks = getValidInput("Enter number of tank players: ", false);
    int healers = getValidInput("Enter number of healer players: ", false);
    int dps = getValidInput("Enter number of DPS players: ", false);
    int t1 = getValidInput("Enter minimum dungeon run time (t1 in seconds): ", true);

    int t2;
    while (true) {
        t2 = getValidInput("Enter maximum dungeon run time (t2 in seconds): ", true);
        if (t2 < t1) {
            cout << "Maximum dungeon run time must be greater than or equal to the minimum run time (" << t1 << " seconds).\n";
            continue;
        }
        break;
    }

    // Calculate number of full parties that can be formed (each party needs 1 tank, 1 healer, 3 DPS).
    int numParties = min({ tanks, healers, dps / 3 });
    cout << "\nNumber of parties that can be formed: " << numParties << "\n";

    // Initialize instance structures and the available instance queue.
    instances.resize(n);
    for (int i = 0; i < n; i++) {
        instances[i].id = i;
        instances[i].active = false;
        instances[i].partiesServed = 0;
        instances[i].totalTimeServed = 0;
        availableInstances.push(i);
    }

    // Launch party threads – one thread per party.
    vector<thread> partyThreads;
    for (int i = 0; i < numParties; i++) {
        partyThreads.push_back(thread(partyFunction, i + 1, t1, t2));
    }

    // Status thread: periodically displays the status of all instances.
    bool running = true;
    thread statusThread([&]() {
        while (running) {
            {
                unique_lock<mutex> lock(mtx);
                cout << "\nInstance Status:\n";
                for (const auto& inst : instances) {
                    cout << "Instance " << inst.id << ": "
                        << (inst.active ? "active" : "empty") << "\n";
                }
            }
            this_thread::sleep_for(chrono::seconds(1));
        }
        });

    // Wait for all party threads to complete.
    for (auto& t : partyThreads) {
        t.join();
    }

    // Stop the status thread.
    running = false;
    statusThread.join();

    // Print final summary.
    cout << "\nFinal Summary:\n";
    for (const auto& inst : instances) {
        cout << "Instance " << inst.id << " served " << inst.partiesServed
            << " parties, total time served: " << inst.totalTimeServed << " seconds.\n";
    }

    // Calculate and print remaining roles.
    int remainingTanks = tanks - numParties;       // 1 tank used per party
    int remainingHealers = healers - numParties;     // 1 healer used per party
    int remainingDPS = dps - (numParties * 3);         // 3 DPS used per party

    cout << "\nRemaining Roles:" << "\n";
    cout << "Tanks: " << remainingTanks << "\n";
    cout << "Healers: " << remainingHealers << "\n";
    cout << "DPS: " << remainingDPS << "\n";

    return 0;
}