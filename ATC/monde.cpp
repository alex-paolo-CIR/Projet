#include "monde.h"
#include "utilitaires.h"
#include <random>

monde::monde() {
    aeroports.push_back({"Paris", {400.f, 150.f}, {}});
    aeroports.push_back({"Lyon", {500.f, 350.f}, {}});
    aeroports.push_back({"Marseille", {500.f, 500.f}, {}});
    aeroports.push_back({"Bordeaux", {260.f, 400.f}, {}}); 
    aeroports.push_back({"Strasbourg", {650.f, 150.f}, {}});

    // creation de trafic initial
    for (int i=0; i<10; ++i) {
        int index_depart = rand() % aeroports.size();
        int index_arrivee = rand() % aeroports.size();
        while(index_arrivee == index_depart) index_arrivee = rand() % aeroports.size();

        auto p = make_shared<avion>(i+1, aeroports[index_depart].position_carte);
        p->index_aeroport_cible = index_arrivee;
        p->cible_carte = aeroports[index_arrivee].position_carte;
        avions_transit.push_back(p);
    }
}

void monde::mettre_a_jour(float dt) {
    // update aeroports
    for (auto& a : aeroports) {
        a.tour.mettre_a_jour(dt);
        
        // recuperer les avions qui decollent
        for (auto& p : a.tour.avions_partis) {
            p->etat = etat_avion::en_transit;
            p->position_carte = a.position_carte;
            
            // choisir nouvelle destination
            int index_suivant = rand() % aeroports.size();
            // on s assure que ce n est pas le meme aeroport simple check d adresse ou nom
            while(aeroports[index_suivant].nom == a.nom) index_suivant = rand() % aeroports.size();
            
            p->index_aeroport_cible = index_suivant;
            p->cible_carte = aeroports[index_suivant].position_carte;
            avions_transit.push_back(p);
        }
        a.tour.avions_partis.clear();
    }

    // update transit
    for (auto& p : avions_transit) {
        p->mettre_a_jour(dt);
        
        // check arrivee
        if (utilitaires::distance(p->position_carte, p->cible_carte) < 5.f) {
            // arrive
            aeroports[p->index_aeroport_cible].tour.ajouter_avion(p);
            p->etat = etat_avion::en_approche; // sera gere par ajouter_avion
        }
    }
    
    // nettoyage transit ceux qui sont passes dans un aeroport
    erase_if(avions_transit, [](const auto& p) { return p->etat != etat_avion::en_transit; });
}

void monde::dessiner_carte(RenderWindow& fenetre, const Font& police, const textures_jeu& textures) {
    // fond france
    if (textures.a_carte) {
        Sprite carte(textures.carte);
        // adapter a la fenetre
        carte.setScale({
            static_cast<float>(fenetre.getSize().x) / carte.getLocalBounds().size.x,
            static_cast<float>(fenetre.getSize().y) / carte.getLocalBounds().size.y
        });
        fenetre.draw(carte);
    } else {
        ConvexShape france;
        france.setPointCount(6);
        france.setPoint(0, {300.f, 50.f});
        france.setPoint(1, {600.f, 50.f});
        france.setPoint(2, {700.f, 200.f});
        france.setPoint(3, {600.f, 550.f});
        france.setPoint(4, {200.f, 550.f});
        france.setPoint(5, {100.f, 250.f});
        france.setFillColor(Color(30, 30, 60));
        fenetre.draw(france);
    }

    // aeroports
    /* les points et noms sont masques car présents sur la texture de fond mais je laisse ici
    for (size_t i=0; i<aeroports.size(); ++i) {
        CircleShape point(8.f);
        point.setOrigin({8.f, 8.f});
        point.setPosition(aeroports[i].position_carte);
        point.setFillColor(Color::Cyan);
        fenetre.draw(point);
    }
    */

    for (auto& p : avions_transit) {
        p->dessiner_carte(fenetre, textures);
    }
}

void monde::gerer_entree(Vector2f pos_souris) {
    if (index_aeroport_selectionne == -1) {
        // clic sur carte selection aeroport
        for (size_t i=0; i<aeroports.size(); ++i) {
            if (utilitaires::distance(pos_souris, aeroports[i].position_carte) < 20.f) {
                index_aeroport_selectionne = static_cast<int>(i);
                break;
            }
        }
    }
}
