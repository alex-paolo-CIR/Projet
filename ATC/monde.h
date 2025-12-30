#pragma once
#include <vector>
#include <string>
#include <memory>
#include <SFML/Graphics.hpp>
#include "tour_controle.h"
#include "avion.h"
#include "textures_jeu.h"

using namespace std;
using namespace sf;

struct aeroport {
    string nom;
    Vector2f position_carte;
    tour_controle tour;
};


class monde {
public:
    vector<aeroport> aeroports;
    vector<shared_ptr<avion>> avions_transit;
    int index_aeroport_selectionne = -1; 

    monde();

    void mettre_a_jour(float dt);
    void dessiner_carte(RenderWindow& fenetre, const Font& police, const textures_jeu& textures);
    void gerer_entree(Vector2f pos_souris);
};
