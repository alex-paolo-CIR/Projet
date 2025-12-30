#include "avion.h"


avion::avion(int id, Vector2f pos_depart_carte) 
    : id(id), position_carte(pos_depart_carte), position({-100.f, -100.f}), rotation(0.f), carburant(50.f + (rand() % 50)), etat(etat_avion::en_transit) {
}

void avion::mettre_a_jour(float dt) {
    // gestion du carburant
    if (etat == etat_avion::stationne) {
        carburant += dt * 10.f; 
        if (carburant > 100) carburant = 100;
    } else {
        carburant -= dt * 0.2f; 
        if (carburant < 0) carburant = 0;
    }

    if (etat == etat_avion::en_transit) {
        mettre_a_jour_transit(dt);
    } else {
        mettre_a_jour_local(dt);
    }
}

void avion::mettre_a_jour_transit(float dt) {
    Vector2f dir = cible_carte - position_carte;
    float dist = utilitaires::longueur(dir);
    
    if (dist > 5.f) {
        Vector2f deplacement = utilitaires::normaliser(dir) * 40.f * dt; // vitesse sur la carte
        position_carte += deplacement;
        rotation = atan2(dir.y, dir.x);
    } else {
        // arrive a l aeroport cible transition geree par le monde
    }
}

void avion::mettre_a_jour_local(float dt) {
    if (chemin.empty()) return;

    Vector2f cible = chemin.front();
    Vector2f dir = cible - position;
    float dist = utilitaires::longueur(dir);

    float vitesse_actuelle = 0.f;
    switch (etat) {
        case etat_avion::en_approche: vitesse_actuelle = 100.f; break;
        case etat_avion::en_attente:     vitesse_actuelle = 80.f; break;
        case etat_avion::alignement:    vitesse_actuelle = 90.f; break;
        case etat_avion::atterrissage:     vitesse_actuelle = 100.f; break;
        case etat_avion::roulage_vers_porte:  vitesse_actuelle = 40.f; break;
        case etat_avion::roulage_vers_piste:vitesse_actuelle = 40.f; break;
        case etat_avion::decollage:   vitesse_actuelle = 150.f; break;
        default: vitesse_actuelle = 0.f; break;
    }

    if (dist > 5.f) {
        Vector2f deplacement = utilitaires::normaliser(dir) * vitesse_actuelle * dt;
        position += deplacement;
        rotation = atan2(dir.y, dir.x);
    } else {
        chemin.pop_front();
        if (chemin.empty()) sur_fin_chemin();
    }
}

void avion::sur_fin_chemin() {
    if (etat == etat_avion::en_approche) {
        etat = etat_avion::en_attente;
        index_attente = 1;
        chemin.push_back(constantes::points_attente[index_attente]);
    }
    else if (etat == etat_avion::en_attente) {
        index_attente = (index_attente + 1) % constantes::points_attente.size();
        chemin.push_back(constantes::points_attente[index_attente]);
    }
    else if (etat == etat_avion::alignement) {
        etat = etat_avion::atterrissage;
        chemin.push_back({constantes::piste_fin_x, constantes::piste_atterrissage_y});
    }
}

void avion::dessiner_local(RenderWindow& fenetre, const Font& police, const textures_jeu& textures) {
    // calcul de la taille dynamique pour l effet de perspective altitude
    float taille_actuelle = 60.f; // taille au sol standard

    if (etat == etat_avion::en_approche || etat == etat_avion::en_attente) {
        taille_actuelle = 35.f; // en altitude plus petit
    }
    else if (etat == etat_avion::alignement) {
        // phase d approche finale l avion grossit en descendant vers la piste
        float dist = utilitaires::distance(position, {constantes::piste_debut_x, constantes::piste_atterrissage_y});
        // interpolation a 300px ou plus taille 35 a 0px taille 60
        float facteur = clamp(dist / 300.f, 0.f, 1.f);
        taille_actuelle = 60.f - (25.f * facteur);
    }
    else if (etat == etat_avion::decollage) {
        // phase de decollage l avion retrecit en montant
        // on considere qu il decolle vers le milieu de la piste
        float x_decollage = (constantes::piste_debut_x + constantes::piste_fin_x) / 2.f;
        if (position.x > x_decollage) {
            float dist = position.x - x_decollage;
            float facteur = clamp(dist / 400.f, 0.f, 1.f);
            taille_actuelle = 60.f - (25.f * facteur);
        }
    }
    else if (etat == etat_avion::parti) {
        taille_actuelle = 35.f;
    }

    if (textures.a_avion) {
        Sprite sprite(textures.avion);
        // important centrer l origine pour que la rotation se fasse autour du centre
        FloatRect limites = sprite.getLocalBounds();
        sprite.setOrigin({limites.size.x / 2.f, limites.size.y / 2.f});
        sprite.setPosition(position);
        sprite.setRotation(sf::radians(rotation));
        
        // mise a l echelle avec la taille dynamique
        float echelle = taille_actuelle / max(limites.size.x, limites.size.y);
        sprite.setScale({echelle, echelle});
        
        // couleur pour indiquer l etat filtre
        if (carburant < 10.f) sprite.setColor(Color(255, 100, 100));
        else if (etat == etat_avion::stationne) sprite.setColor(Color(200, 200, 255));
        
        fenetre.draw(sprite);
    } else {
        // fallback geometrique
        ConvexShape forme;
        forme.setPointCount(3);
        
        // adaptation de la taille du shape geometrique
        float s = taille_actuelle / 60.f; // ratio par rapport a la taille standard
        forme.setPoint(0, {15.f * s, 0.f});
        forme.setPoint(1, {-10.f * s, -10.f * s});
        forme.setPoint(2, {-10.f * s, 10.f * s});
        
        if (carburant < 10.f) forme.setFillColor(Color::Red);
        else if (etat == etat_avion::stationne) forme.setFillColor(Color::Blue);
        else forme.setFillColor(Color::White);

        forme.setPosition(position);
        forme.setRotation(sf::radians(rotation));
        fenetre.draw(forme);
    }

    Text texte(police);
    texte.setString(format("ID:{}", id));
    texte.setCharacterSize(10);
    texte.setFillColor(Color::Yellow);
    texte.setPosition(position + Vector2f(-10.f, -20.f));
    fenetre.draw(texte);
}

void avion::dessiner_carte(RenderWindow& fenetre, const textures_jeu& textures) {
    if (textures.a_avion) {
        Sprite sprite(textures.avion);
        FloatRect limites = sprite.getLocalBounds();
        sprite.setOrigin({limites.size.x / 2.f, limites.size.y / 2.f});
        sprite.setPosition(position_carte);
        sprite.setRotation(sf::radians(rotation));
        float echelle = 20.f / max(limites.size.x, limites.size.y); // plus petit sur la carte
        sprite.setScale({echelle, echelle});
        sprite.setColor(Color::Magenta); // teinte pour differencier
        fenetre.draw(sprite);
    } else {
        ConvexShape forme;
        forme.setPointCount(3);
        forme.setPoint(0, {8.f, 0.f});
        forme.setPoint(1, {-5.f, -5.f});
        forme.setPoint(2, {-5.f, 5.f});
        forme.setFillColor(Color::Magenta);
        forme.setPosition(position_carte);
        forme.setRotation(sf::radians(rotation));
        fenetre.draw(forme);
    }
}
