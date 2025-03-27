#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <chrono>
#include <random>
#include <algorithm>

using namespace std;

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
    int n, tanks, healers, dps, t1, t2;

    // Get inputs from the user.
    cout << "Enter maximum number of concurrent instances (n): ";
    cin >> n;
    cout << "Enter number of tank players: ";
    cin >> tanks;
    cout << "Enter number of healer players: ";
    cin >> healers;
    cout << "Enter number of DPS players: ";
    cin >> dps;
    cout << "Enter minimum dungeon run time (t1 in seconds): ";
    cin >> t1;
    cout << "Enter maximum dungeon run time (t2 in seconds): ";
    cin >> t2;

    // Calculate number of full parties that can be formed.
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

    return 0;
}
