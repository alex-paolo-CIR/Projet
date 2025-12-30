#pragma once
#include <SFML/Graphics.hpp>
#include <deque>
#include <vector>
#include <cmath>
#include <algorithm>
#include <format>
#include "constantes.h"
#include "utilitaires.h"
#include "textures_jeu.h"

using namespace std;
using namespace sf;


enum class etat_avion {
    en_transit,      // en vol entre deux aeroports vue carte
    en_approche,    // en approche de l aeroport vue locale
    en_attente,        // en attente d atterrissage
    alignement,       // alignement final
    atterrissage,        // sur la piste d atterrissage
    roulage_vers_porte,     // roulage vers le parking
    stationne,         // au parking
    roulage_vers_piste,   // roulage vers la piste de decollage
    attente_piste,   // en attente de decollage
    decollage,      // sur la piste de decollage
    parti        // a quitte la zone locale retourne en transit
};

// classe avion
// pas de majuscules pas d accent pas de ponctuation

class avion {
public:
    int id;
    
    // position locale dans l aeroport
    Vector2f position;
    // position globale sur la carte
    Vector2f position_carte;
    
    float rotation;
    float carburant; 
    etat_avion etat;
    
    // navigation locale
    deque<Vector2f> chemin;
    int index_porte_assignee = -1;
    int index_attente = 0;

    // navigation globale
    int index_aeroport_cible = -1;
    Vector2f cible_carte;

    avion(int id, Vector2f pos_depart_carte);

    void mettre_a_jour(float dt);
    void mettre_a_jour_transit(float dt);
    void mettre_a_jour_local(float dt);
    void sur_fin_chemin();

    void dessiner_local(RenderWindow& fenetre, const Font& police, const textures_jeu& textures);
    void dessiner_carte(RenderWindow& fenetre, const textures_jeu& textures);
};
