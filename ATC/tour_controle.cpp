#include "tour_controle.h"


tour_controle::tour_controle() {
    for(int i=0; i<3; ++i) {
        portes.push_back({300.f + i * 100.f, constantes::porte_y});
        porte_occupee.push_back(false);
    }
}

void tour_controle::ajouter_avion(shared_ptr<avion> p) {
    p->etat = etat_avion::en_approche;
    p->position = {-50.f, 50.f}; 
    p->chemin.clear();
    p->chemin.push_back(constantes::points_attente[0]);
    avions.push_back(p);
    file_atterrissage.push_back(p->id);
}

void tour_controle::mettre_a_jour(float dt) {
    // 1 gestion piste atterrissage
    if (!piste_atterrissage_occupee && !file_atterrissage.empty()) {
        int id = file_atterrissage.front();
        auto it = find_if(avions.begin(), avions.end(), [id](auto& p){ return p->id == id; });
        if (it != avions.end()) {
            avion& p = **it;
            int porte_libre = -1;
            for(size_t i=0; i<portes.size(); ++i) {
                if (!porte_occupee[i]) { porte_libre = static_cast<int>(i); break; }
            }

            if (porte_libre != -1 && p.etat == etat_avion::en_attente) {
                p.index_porte_assignee = porte_libre;
                porte_occupee[porte_libre] = true;
                p.etat = etat_avion::alignement;
                p.chemin.clear();
                p.chemin.push_back({constantes::piste_debut_x - 100.f, constantes::piste_atterrissage_y});
                p.chemin.push_back({constantes::piste_debut_x, constantes::piste_atterrissage_y});
                piste_atterrissage_occupee = true;
                file_atterrissage.pop_front();
            }
        }
    } 
    
    // 2 gestion piste decollage
    if (!piste_decollage_occupee && !file_decollage.empty()) {
        int id = file_decollage.front();
        auto it = find_if(avions.begin(), avions.end(), [id](auto& p){ return p->id == id; });
        if (it != avions.end()) {
            avion& p = **it;
            if (p.etat == etat_avion::attente_piste) {
                p.etat = etat_avion::decollage;
                p.chemin.clear();
                p.chemin.push_back({constantes::piste_fin_x, constantes::piste_decollage_y});
                p.chemin.push_back({constantes::piste_fin_x + 300.f, constantes::piste_decollage_y - 100.f});
                piste_decollage_occupee = true;
                file_decollage.pop_front();
            }
        }
    }

    // 3 mise a jour
    for (auto& p : avions) {
        p->mettre_a_jour(dt);

        if (p->chemin.empty()) {
            if (p->etat == etat_avion::atterrissage) {
                piste_atterrissage_occupee = false;
                p->etat = etat_avion::roulage_vers_porte;
                p->chemin.push_back({p->position.x, constantes::porte_y});
                if (p->index_porte_assignee >= 0 && p->index_porte_assignee < portes.size()) p->chemin.push_back(portes[p->index_porte_assignee]);
            }
            else if (p->etat == etat_avion::roulage_vers_porte) {
                p->etat = etat_avion::stationne;
                p->rotation = constantes::pi / 2.f;
            }
            else if (p->etat == etat_avion::stationne && p->carburant >= 100.f) {
                p->etat = etat_avion::roulage_vers_piste;
                if (p->index_porte_assignee >= 0 && p->index_porte_assignee < porte_occupee.size()) porte_occupee[p->index_porte_assignee] = false;
                p->index_porte_assignee = -1;
                p->chemin.push_back({p->position.x, constantes::piste_decollage_y - 60.f});
                p->chemin.push_back({constantes::piste_debut_x, constantes::piste_decollage_y - 60.f});
                p->chemin.push_back({constantes::piste_debut_x, constantes::piste_decollage_y});
            }
            else if (p->etat == etat_avion::roulage_vers_piste) {
                p->etat = etat_avion::attente_piste;
                file_decollage.push_back(p->id);
            }
            else if (p->etat == etat_avion::decollage) {
                piste_decollage_occupee = false;
                p->etat = etat_avion::parti;
            }
        }
        
        if (p->etat == etat_avion::decollage && piste_decollage_occupee && p->position.x > constantes::piste_fin_x) {
            piste_decollage_occupee = false;
        }
    }

    // transfert des avions partis
    for (auto& p : avions) {
        if (p->etat == etat_avion::parti) {
            avions_partis.push_back(p);
        }
    }
    erase_if(avions, [](const auto& p) { return p->etat == etat_avion::parti; });
}

void tour_controle::dessiner(RenderWindow& fenetre, const Font& police, const textures_jeu& textures) {
    // fond herbe
    if (textures.a_herbe) {
        Sprite herbe(textures.herbe);
        // repetition de la texture tiling pour eviter la pixellisation
        herbe.setTextureRect(IntRect({0, 0}, {static_cast<int>(fenetre.getSize().x), static_cast<int>(fenetre.getSize().y)}));
        fenetre.draw(herbe);
    }

    // piste image unique
    if (textures.a_piste) {
        Sprite s(textures.piste);
        s.setScale({
            static_cast<float>(fenetre.getSize().x) / s.getLocalBounds().size.x,
            static_cast<float>(fenetre.getSize().y) / s.getLocalBounds().size.y
        });
        fenetre.draw(s);
    }

    // portes
    for (size_t i=0; i<portes.size(); ++i) {
        CircleShape porte(4.f);
        porte.setOrigin({4.f, 4.f});
        porte.setPosition(portes[i]);
        porte.setFillColor(porte_occupee[i] ? Color::Red : Color::Green);
        fenetre.draw(porte);
    }

    // tour
    if (textures.a_tour) {
        Sprite t(textures.tour);
        FloatRect b = t.getLocalBounds();
        t.setOrigin({b.size.x / 2.f, b.size.y}); // ancre en bas au centre
        t.setPosition({400.f, 110.f}); // un peu plus haut
        float echelle = 80.f / b.size.y;
        t.setScale({echelle, echelle});
        fenetre.draw(t);
    } else {
        RectangleShape tour({30.f, 60.f});
        tour.setOrigin({15.f, 60.f});
        tour.setPosition({400.f, 50.f});
        tour.setFillColor(Color::Cyan);
        fenetre.draw(tour);
    }

    for (auto& p : avions) p->dessiner_local(fenetre, police, textures);
}
