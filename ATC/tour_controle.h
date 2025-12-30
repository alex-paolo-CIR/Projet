#pragma once
#include <vector>
#include <deque>
#include <memory>
#include <algorithm>
#include <SFML/Graphics.hpp>
#include "avion.h"
#include "constantes.h"
#include "textures_jeu.h"

using namespace std;
using namespace sf;


class tour_controle {
public:
    vector<shared_ptr<avion>> avions;
    deque<int> file_atterrissage;
    deque<int> file_decollage;
    vector<shared_ptr<avion>> avions_partis; 
    bool piste_atterrissage_occupee = false;
    bool piste_decollage_occupee = false;
    vector<Vector2f> portes;
    vector<bool> porte_occupee;

    tour_controle();

    void ajouter_avion(shared_ptr<avion> p);
    void mettre_a_jour(float dt);
    void dessiner(RenderWindow& fenetre, const Font& police, const textures_jeu& textures);
};
