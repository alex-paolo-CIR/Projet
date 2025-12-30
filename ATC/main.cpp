#include <SFML/Graphics.hpp>
#include <optional>
#include <random>
#include <filesystem>
#include "monde.h"
#include "textures_jeu.h"

using namespace std;
using namespace sf;

int main()
{
    RenderWindow fenetre(VideoMode({800, 600}), "Simulation Trafic Aerien France");
    fenetre.setFramerateLimit(60);

    Font police;
    if (!police.openFromFile("C:/Windows/Fonts/arial.ttf")) {
        // fallback
    }

    // chargement des textures
    textures_jeu textures;
    textures.charger();

    monde m;
    Clock horloge;

    while (fenetre.isOpen())
    {
        while (const optional event = fenetre.pollEvent())
        {
            if (event->is<Event::Closed>()) {
                fenetre.close();
            }
            else if (const auto* bouton_souris = event->getIf<Event::MouseButtonPressed>()) {
                if (bouton_souris->button == Mouse::Button::Left) {
                    m.gerer_entree(Vector2f(static_cast<float>(bouton_souris->position.x), static_cast<float>(bouton_souris->position.y)));
                }
                else if (bouton_souris->button == Mouse::Button::Right) {
                    m.index_aeroport_selectionne = -1; // retour carte
                }
            }
            else if (const auto* touche = event->getIf<Event::KeyPressed>()) {
                if (touche->code == Keyboard::Key::Escape) {
                    m.index_aeroport_selectionne = -1;
                }
            }
        }

        float dt = horloge.restart().asSeconds();
        m.mettre_a_jour(dt);

        fenetre.clear(Color::Black);

        if (m.index_aeroport_selectionne == -1) {
            m.dessiner_carte(fenetre, police, textures);
        } else {
            // vue aeroport
            if (!textures.a_herbe) fenetre.clear(Color(50, 150, 50)); 
            m.aeroports[m.index_aeroport_selectionne].tour.dessiner(fenetre, police, textures);
            
            Text titre(police);
            titre.setString("Aeroport: " + m.aeroports[m.index_aeroport_selectionne].nom + " (Echap pour retour)");
            titre.setCharacterSize(20);
            titre.setPosition({10.f, 10.f});
            fenetre.draw(titre);
        }

        fenetre.display();
    }

    return 0;
}
