
#pragma once
#include <SFML/System/Vector2.hpp>
#include <cmath>

using namespace std;

namespace utilitaires {
    inline float longueur(sf::Vector2f v) { 
        return sqrt(v.x * v.x + v.y * v.y); 
    }

    inline sf::Vector2f normaliser(sf::Vector2f v) { 
        float l = longueur(v); 
        return l != 0 ? v / l : v; 
    }

    inline float distance(sf::Vector2f a, sf::Vector2f b) { 
        return longueur(b - a); 
    }
}
