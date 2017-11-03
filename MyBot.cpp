#include <algorithm>
#include <vector>
#include <string>
#include "hlt/hlt.hpp"
#include "hlt/navigation.hpp"

struct TargetPlanet {
    hlt::Planet target;
    double  distanceToTarget;
    TargetPlanet(hlt::Planet p, double d) : target(p), distanceToTarget(d){}
    bool operator() (TargetPlanet i,TargetPlanet j) { return (i.distanceToTarget < j.distanceToTarget);}
};

struct TargetShip {
    hlt::Ship target;
    double distanceToTarget;
    TargetShip(hlt::Ship s, double d) : target(s), distanceToTarget(d){}
    bool operator() (TargetShip i, TargetShip j) { return (i.distanceToTarget < j.distanceToTarget);}
};

int main() {
    const hlt::Metadata metadata = hlt::initialize("C_Bot");
    const hlt::PlayerId player_id = metadata.player_id;

    const hlt::Map& initial_map = metadata.initial_map;

    // We now have 1 full minute to analyse the initial map.
    std::ostringstream initial_map_intelligence;
    initial_map_intelligence
            << "width: " << initial_map.map_width
            << "; height: " << initial_map.map_height
            << "; players: " << initial_map.ship_map.size()
            << "; my ships: " << initial_map.ship_map.at(player_id).size()
            << "; planets: " << initial_map.planets.size();
    hlt::Log::log(initial_map_intelligence.str());
    hlt::Log::log(std::to_string(player_id).c_str());

    std::vector<hlt::Move> moves;
    for (;;) {
        moves.clear();
        const hlt::Map map = hlt::in::get_map();

        for (const hlt::Ship& ship : map.ships.at(player_id)) {
            if (ship.docking_status != hlt::ShipDockingStatus::Undocked) {
                continue;
            }

            std::vector<TargetPlanet> planetsByDistance;
            bool freePlanets = false;
            bool ownedPlanetsFull = true;

            for (const hlt::Planet& planet : map.planets) {
                if(!planet.owned){
                    freePlanets = true;
                }
                if(planet.owner_id == player_id) {
                    if(!planet.is_full()) {
                        ownedPlanetsFull = false;
                    }
                }
                double distance = ship.location.get_distance_to(planet.location);
                planetsByDistance.push_back(*(new TargetPlanet(planet, distance)));
            }

            std::sort(planetsByDistance.begin(), planetsByDistance.end(), [](TargetPlanet a, TargetPlanet b) {
                return  a.distanceToTarget < b.distanceToTarget;   
            }); 

            bool moved = false;
            for (const TargetPlanet targetPlanet: planetsByDistance) {
                const hlt::Planet& planet = targetPlanet.target;

                if (!planet.is_full() && ship.can_dock(planet)) {
                    std::ostringstream logger;
                    logger << "Ship [" << ship.entity_id << "] is docking on planet [" << planet.entity_id << "] owned by player [" << planet.owner_id << "]\n\tDocking on own planet."; 
                    hlt::Log::log(logger.str());
                    moves.push_back(hlt::Move::dock(ship.entity_id, planet.entity_id));
                    moved = true;
                    break;
                }

                if (planet.owned && freePlanets) {
                    continue;
                }

                if (planet.owner_id == player_id && planet.is_full()) {
                    continue;
                }

                if (!freePlanets && ownedPlanetsFull) {
                    if (planet.owner_id == player_id) {
                        continue;
                    }
                    std::vector<hlt::EntityId> dockedShips = planet.docked_ships;
                    hlt::Ship target = map.get_ship(planet.owner_id,dockedShips[0]);
                    std::ostringstream logger;
                    logger << "Ship [" << ship.entity_id << "] is targeting ship [" << target.entity_id << "] owned by player [" << target.owner_id << "]\n\tAttacking planet."; 
                    hlt::Log::log(logger.str()); 
                    const hlt::possibly<hlt::Move> move =
                        hlt::navigation::navigate_ship_towards_target(map, ship, target.location, hlt::constants::MAX_SPEED,
                            true, hlt::constants::MAX_NAVIGATION_CORRECTIONS, M_PI/180);
                    if (move.second) {
                        moves.push_back(move.first);
                    }
                    moved = true;
                    break;
                }

                std::ostringstream logger;
                logger << "Ship [" << ship.entity_id << "] is targeting planet [" << planet.entity_id << "] owned by player [" << planet.owner_id << "]\n\tMoving towards planet to dock."; 
                hlt::Log::log(logger.str());
                const hlt::possibly<hlt::Move> move =
                        hlt::navigation::navigate_ship_to_dock(map, ship, planet, hlt::constants::MAX_SPEED);
                if (move.second) {
                    moves.push_back(move.first);
                }
                moved = true;
                break;
            }
        }

        if (!hlt::out::send_moves(moves)) {
            hlt::Log::log("send_moves failed; exiting");
            break;
        }
    }
}
