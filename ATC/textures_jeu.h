#pragma once
#include <SFML/Graphics.hpp>
#include <vector>
#include <string>
#include <iostream>

using namespace std;
using namespace sf;

struct textures_jeu {
    Texture avion;
    Texture carte;
    Texture herbe; // fond aeroport
    Texture piste;
    Texture tour;
    
    bool a_avion = false;
    bool a_carte = false;
    bool a_herbe = false;
    bool a_piste = false;
    bool a_tour = false;

    void charger();
};
