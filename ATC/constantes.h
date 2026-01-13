#pragma once
#include <numbers>
#include <vector>
#include <SFML/System/Vector2.hpp>

using namespace std;

namespace constantes {
    constexpr float pi = numbers::pi_v<float>;

    // configuration de l aeroport vue locale
    constexpr float piste_atterrissage_y = 250.f; 
    constexpr float piste_decollage_y = 510.f; 
    constexpr float piste_debut_x = 100.f;
    constexpr float piste_fin_x = 700.f;
    constexpr float porte_y = 380.f; 

    // points de passage pour le circuit d attente vue locale
    const vector<sf::Vector2f> points_attente = {
        {50.f, 50.f}, {750.f, 50.f}, {750.f, 550.f}, {50.f, 550.f}
    };
}
