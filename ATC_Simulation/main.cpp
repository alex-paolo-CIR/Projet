#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <memory>
#include <cmath>
#include <random>
#include <string>

#include "Commun.hpp"
#include "Journaliseur.hpp"
#include "TourControle.hpp"
#include "Avion.hpp"
#include "GestionnaireRessources.hpp"

using namespace std;
using namespace sf;

// chemins ressources
#ifdef PATH_IMG
const string CHEMIN_IMG = string(PATH_IMG);
#else
const string CHEMIN_IMG = "img/";
#endif

// generateur aleatoire spawn
random_device dispAleatoire;
mt19937 gen(dispAleatoire());
uniform_real_distribution<float> distX(100.0f, 700.0f);
uniform_real_distribution<float> distVitesse(50.0f, 70.0f);

// definir nombre davions au demarrage
const int NB_AVIONS_DEPART = 5;

// couleur selon etat avion
Color obtenirCouleurPourEtat(EtatAvion etat) {
    switch (etat) {
  case EtatAvion::APPROCHE:
            return Color::Yellow;
   case EtatAvion::ATTENTE:
            return Color(255, 165, 0);
      case EtatAvion::ATTERRISSAGE:
        case EtatAvion::DECOLLAGE:
            return Color::Red;
        case EtatAvion::ATTENTE_CROISEMENT:
      return Color(255, 200, 0);  // orange clair attend croisement
        case EtatAvion::CROISEMENT:
     return Color(255, 100, 255);  // rose traverse
        case EtatAvion::ROULAGE_ENTREE:
        case EtatAvion::ROULAGE_SORTIE:
      return Color::Green;
        case EtatAvion::STATIONNE:
  return Color(0, 200, 0);
        case EtatAvion::DEPART:
    return Color(100, 200, 255);
        case EtatAvion::URGENCE:
            return Color(255, 0, 0);
    case EtatAvion::ECRASE:
 return Color(30, 30, 30);
   default:
            return Color::White;
    }
}

// dessiner jauge carburant
void dessinerBarreCarburant(RenderWindow& fenetre, const Position& pos, float pourcentageCarburant) {
    const float largeurBarre = 50.0f;
    const float hauteurBarre = 6.0f;
    const float decalageY = -30.0f;
 
    // fond barre
  RectangleShape arrierePlan(Vector2f(largeurBarre, hauteurBarre));
    arrierePlan.setPosition(Vector2f(pos.x - largeurBarre / 2, pos.y + decalageY));
    arrierePlan.setFillColor(Color(30, 30, 30));
    arrierePlan.setOutlineColor(Color::White);
    arrierePlan.setOutlineThickness(1);
    fenetre.draw(arrierePlan);
    
    // barre carburant
    float largeurActuelle = (largeurBarre - 2) * (pourcentageCarburant / 100.0f);
    if (largeurActuelle > 0) {
        RectangleShape barreCarburant(Vector2f(largeurActuelle, hauteurBarre - 2));
  barreCarburant.setPosition(Vector2f(pos.x - largeurBarre / 2 + 1, pos.y + decalageY + 1));
        
        // couleur vert jaune rouge
        if (pourcentageCarburant > 50.0f) {
            barreCarburant.setFillColor(Color::Green);
        } else if (pourcentageCarburant > 20.0f) {
            barreCarburant.setFillColor(Color::Yellow);
        } else {
     barreCarburant.setFillColor(Color::Red);
        }
        
        fenetre.draw(barreCarburant);
    }
}

// dessiner legende
void dessinerLegende(RenderWindow& fenetre, const Font& police) {
    float departX = 10.0f;
    float departY = 10.0f;
    float tailleCarre = 15.0f;
    float espacement = 22.0f;
    
  struct EntreeLegende {
   string etiquette;
        Color couleur;
    };
    
    vector<EntreeLegende> entrees = {
        {"approche", Color::Yellow},
   {"attente", Color(255, 165, 0)},
  {"atterrissage", Color::Red},
        {"croisement", Color(255, 100, 255)},  // rose pour croisement
  {"roulage", Color::Green},
        {"parking", Color(0, 200, 0)},
        {"decollage", Color::Cyan},
        {"depart", Color(100, 200, 255)},
  {"urgence", Color(255, 0, 0)},
        {"crash", Color(30, 30, 30)}
    };
    
    for (int i = 0; i < (int)entrees.size(); ++i) {
        RectangleShape carreCouleur(Vector2f(tailleCarre, tailleCarre));
        carreCouleur.setPosition(Vector2f(departX, departY + i * espacement));
        carreCouleur.setFillColor(entrees[i].couleur);
 carreCouleur.setOutlineColor(Color::White);
        carreCouleur.setOutlineThickness(1);
        fenetre.draw(carreCouleur);
   
        Text texte(police);
        texte.setString(entrees[i].etiquette);
        texte.setCharacterSize(12);
     texte.setFillColor(Color::White);
        texte.setPosition(Vector2f(departX + tailleCarre + 5, departY + i * espacement - 2));
        fenetre.draw(texte);
    }
}

// dessiner statistiques
void dessinerStatistiques(RenderWindow& fenetre, const Font& police, const TourControle& tour, 
    int avionsActifs, bool pisteFermee) {
    float departX = 10.0f;
    float departY = 550.0f;
    
    Text texteStats(police);
 texteStats.setCharacterSize(14);
    texteStats.setFillColor(Color::White);
    
    string stats = "actifs " + to_string(avionsActifs) + 
   " attente " + to_string(tour.obtenirAvionsEnAttente()) +
          " atterr " + to_string(tour.obtenirTotalAtterrissages()) +
        " decol " + to_string(tour.obtenirTotalDecollages()) +
            " crash " + to_string(tour.obtenirTotalCrashs());
    
    if (pisteFermee) {
        stats += " piste fermee";
    }
    
    texteStats.setString(stats);
    texteStats.setPosition(Vector2f(departX, departY));
fenetre.draw(texteStats);
}

// dessiner controles
void dessinerControles(RenderWindow& fenetre, const Font& police) {
    float departX = 10.0f;
    float departY = 570.0f;
    
  Text texteControles(police);
    texteControles.setCharacterSize(12);
    texteControles.setFillColor(Color(200, 200, 200));
    texteControles.setString("espace spawn entree fermer piste c test crash echap quitter");
    texteControles.setPosition(Vector2f(departX, departY));
    fenetre.draw(texteControles);
}

// creer nouvel avion position aleatoire
unique_ptr<Avion> genererNouvelAvion(int& compteurIdAvion, TourControle* tour) {
    float x = distX(gen);
    float vitesse = distVitesse(gen);
    
    auto avion = make_unique<Avion>(compteurIdAvion++, tour, Position(x, -50), vitesse);
    avion->demarrer();
    
    Journaliseur::obtenirInstance().journaliser("Main", "nouvel avion genere id " + to_string(compteurIdAvion - 1), "INFO");
    return avion;
}

// creer avion test crash calcul mathematique precis
unique_ptr<Avion> genererAvionTestCrash(int& compteurIdAvion, TourControle* tour) {
    // position depart haut ecran
    float departX = 400.0f;
    float departY = -50.0f;
    Position posDepart(departX, departY);
 
    // vitesse avion
    float vitesse = 60.0f;
    
    // calcul distance jusqua milieu piste GAUCHE
Position pointApproche = tour->obtenirPointApproche();
    Position seuilPiste = tour->obtenirSeuilPisteGauche();  // piste GAUCHE
    Position milieuPiste(400.0f, 280.0f);  // milieu piste gauche
    
    // distance totale parcourir
  float distanceVersApproche = posDepart.distanceVers(pointApproche);
    float distanceApprocheVersSeuil = pointApproche.distanceVers(seuilPiste);
    float distanceSeuilVersMilieu = seuilPiste.distanceVers(milieuPiste);
    
 float distanceTotale = distanceVersApproche + distanceApprocheVersSeuil + distanceSeuilVersMilieu;
    
    // temps vol necessaire
 float tempsVersMilieu = distanceTotale / vitesse;
    
    // carburant necessaire avec buffer
    float carburantNecessaire = tempsVersMilieu * ConfigCarburant::TAUX_CONSOMMATION_CARBURANT;
    
    // creer avion
    auto avion = make_unique<Avion>(compteurIdAvion++, tour, posDepart, vitesse);
    avion->definirCarburant(carburantNecessaire * 1.05f);
    avion->demarrer();
    
    stringstream ss;
    ss << "avion test crash genere id " << (compteurIdAvion - 1) 
       << " distance " << distanceTotale 
       << " temps " << tempsVersMilieu 
     << " carburant " << (carburantNecessaire * 1.05f);
    Journaliseur::obtenirInstance().journaliser("Main", ss.str(), "WARN");
    
    return avion;
}

int main() {
    try {
        // init fenetre sfml
        RenderWindow fenetre(VideoMode({800, 600}), "simulation tour controle amelioree v2");
     fenetre.setFramerateLimit(60);
        
        // charger police
 Font police("C:/Windows/Fonts/arial.ttf");
        
        // charger ressources graphiques
        GestionnaireRessources& gestionnaireRes = GestionnaireRessources::obtenirInstance();
      const Texture* textureAvion = gestionnaireRes.chargerTexture(CHEMIN_IMG + "avion.png");
        const Texture* textureFond = gestionnaireRes.chargerTexture(CHEMIN_IMG + "fond.png");

        // sprite fond
        unique_ptr<Sprite> spriteFond;
        if (textureFond) {
      spriteFond = make_unique<Sprite>(*textureFond);
  Vector2u tailleTexture = textureFond->getSize();
            if (tailleTexture.x > 0 && tailleTexture.y > 0) {
float echelleX = 800.0f / tailleTexture.x;
                float echelleY = 600.0f / tailleTexture.y;
      spriteFond->setScale(Vector2f(echelleX, echelleY));
          }
      }
        
        // init tour controle
   TourControle tour(5);
        
    // scenario charge lourde 3 avions au demarrage espaces verticalement
    vector<unique_ptr<Avion>> avions;
  int compteurIdAvion = 1;
        
        Journaliseur::obtenirInstance().journaliser("Main", "scenario charge lourde 3 avions", "INFO");
        
        for (int i = 0; i < NB_AVIONS_DEPART; ++i) {
   // position y espacee
         float departY = -50.0f - (i * 150.0f);
       // position x variee
         float departX = 350.0f + (i % 3) * 50.0f;
  // vitesse variee
        float vitesse = 55.0f + (i % 3) * 5.0f;
   
            auto avion = make_unique<Avion>(compteurIdAvion++, &tour, Position(departX, departY), vitesse);
            avion->demarrer();
       avions.push_back(move(avion));

   stringstream ss;
   ss << "charge lourde avion " << (compteurIdAvion - 1) << " y " << departY;
      Journaliseur::obtenirInstance().journaliser("Main", ss.str(), "INFO");
        }

        Journaliseur::obtenirInstance().journaliser("Main", "simulation demarree 3 avions espaces", "INFO");
        
    // horloge effets clignotement
        Clock horlogeClignotement;
    bool pisteFermee = false;
        
 // boucle principale
        while (fenetre.isOpen()) {
    // gestion evenements
     while (const optional evenement = fenetre.pollEvent()) {
           if (evenement->is<Event::Closed>()) {
          fenetre.close();
   }

    if (const auto* toucheAppuyee = evenement->getIf<Event::KeyPressed>()) {
         if (toucheAppuyee->code == Keyboard::Key::Escape) {
               fenetre.close();
   }
     // espace spawner nouvel avion
             else if (toucheAppuyee->code == Keyboard::Key::Space) {
            avions.push_back(genererNouvelAvion(compteurIdAvion, &tour));
       }
         // entree fermer ouvrir pistes
        else if (toucheAppuyee->code == Keyboard::Key::Enter) {
   if (pisteFermee) {
        tour.ouvrirPistes();
   pisteFermee = false;
       } else {
   tour.fermerPistes();
     pisteFermee = true;
  }
       }
       // c spawner avion test crash calcul precis
           else if (toucheAppuyee->code == Keyboard::Key::C) {
        avions.push_back(genererAvionTestCrash(compteurIdAvion, &tour));
            }
    }
            }
            
   // rendu
     fenetre.clear(Color(20, 20, 40));
            
     // dessiner fond si disponible
       if (spriteFond) {
         fenetre.draw(*spriteFond);
      }
       
 // dessiner DEUX pistes VERTICALES paralleles
  // Piste GAUCHE (atterrissage) - x=350 verticale
    RectangleShape pisteGauche(Vector2f(30, 500));  // largeur 30, hauteur 550 (verticale)
    pisteGauche.setPosition(Vector2f(335, 50));      // x=335 pour centrer sur 350
    pisteGauche.setFillColor(pisteFermee ? Color(150, 0, 0) : Color(70, 70, 70));
    pisteGauche.setOutlineColor(Color::White);
    pisteGauche.setOutlineThickness(2);
    fenetre.draw(pisteGauche);
  
    // Piste DROITE (decollage) - x=450 verticale
 RectangleShape pisteDroite(Vector2f(30, 500));  // largeur 30, hauteur 550 (verticale)
 pisteDroite.setPosition(Vector2f(435, 50));      // x=435 pour centrer sur 450
    pisteDroite.setFillColor(pisteFermee ? Color(150, 0, 0) : Color(90, 90, 90));
    pisteDroite.setOutlineColor(Color::White);
    pisteDroite.setOutlineThickness(2);
  fenetre.draw(pisteDroite);
     
   // marquages pistes verticales
    if (!pisteFermee) {
  // marquages piste gauche atterrissage (verticaux)
        for (int i = 0; i < 10; ++i) {
     RectangleShape marquage(Vector2f(6, 30));  // marquages verticaux
     marquage.setPosition(Vector2f(342.0f, 50.0f + i * 50));
    marquage.setFillColor(Color::Yellow);
            fenetre.draw(marquage);
     }
     
 // marquages piste droite decollage (verticaux)
        for (int i = 0; i < 10; ++i) {
     RectangleShape marquage(Vector2f(6, 30));  // marquages verticaux
 marquage.setPosition(Vector2f(442.0f, 50.0f + i * 50));
  marquage.setFillColor(Color::Cyan);
         fenetre.draw(marquage);
       }
  } else {
  // croix rouge pistes fermees (verticales)
    RectangleShape croix1(Vector2f(600, 5));
   croix1.setFillColor(Color::Red);
        croix1.setRotation(degrees(90));
        croix1.setOrigin(Vector2f(300, 2.5f));
        croix1.setPosition(Vector2f(350, 300));
       fenetre.draw(croix1);
  
        RectangleShape croix2(Vector2f(600, 5));
        croix2.setFillColor(Color::Red);
        croix2.setRotation(degrees(90));
        croix2.setOrigin(Vector2f(300, 2.5f));
        croix2.setPosition(Vector2f(450, 300));
   fenetre.draw(croix2);
   }

         // dessiner parkings
            for (int i = 0; i < 5; ++i) {
            Position posParking = tour.obtenirPositionParking(i);
            CircleShape placeParking(15);
            placeParking.setPosition(Vector2f(posParking.x - 15, posParking.y - 15));
            placeParking.setFillColor(Color(60, 60, 60));
            placeParking.setOutlineColor(Color(150, 150, 150));
            placeParking.setOutlineThickness(2);
            fenetre.draw(placeParking);
            }
      
      // dessiner avions
            int avionsActifs = 0;
      bool clignotement = (horlogeClignotement.getElapsedTime().asMilliseconds() % 1000) < 500;
        
     for (auto& avion : avions) {
    if (avion->estActif()) {
      avionsActifs++;
          
        Position pos = avion->obtenirPosition();
    EtatAvion etat = avion->obtenirEtat();
  float rotation = avion->obtenirAngleRotation();
   
       // utiliser sprite ou forme par defaut
          if (textureAvion && etat != EtatAvion::ECRASE) {
               Sprite spriteAvion(*textureAvion);
      
     // centrer origine
   Vector2u tailleTexture = textureAvion->getSize();
           spriteAvion.setOrigin(Vector2f(tailleTexture.x / 2.0f, tailleTexture.y / 2.0f));
    
         // position et rotation
         spriteAvion.setPosition(Vector2f(pos.x, pos.y));
    spriteAvion.setRotation(degrees(rotation + 90.0f));
                
  // echelle agrandie
        spriteAvion.setScale(Vector2f(0.15f, 0.15f));
               
      // couleur selon etat
         if (etat == EtatAvion::URGENCE && clignotement) {
                spriteAvion.setColor(Color::Red);
       } else {
                spriteAvion.setColor(Color::White);
            }
              
     fenetre.draw(spriteAvion);
      } else {
           // fallback cercle
   float rayon = (etat == EtatAvion::ECRASE ? 12.0f : 15.0f);
     CircleShape formeAvion(rayon);
  formeAvion.setPosition(Vector2f(pos.x - rayon, pos.y - rayon));
              
      // couleur avec clignotement urgence
            Color couleur = obtenirCouleurPourEtat(etat);
          if (etat == EtatAvion::URGENCE && clignotement) {
        couleur = Color(255, 100, 100);
              }
   
           // forcer gris fonce crash
             if (etat == EtatAvion::ECRASE) {
         couleur = Color(30, 30, 30);
         }
              
     formeAvion.setFillColor(couleur);
           formeAvion.setOutlineColor(Color::White);
          formeAvion.setOutlineThickness(1);
             fenetre.draw(formeAvion);
             }
    
           // jauge carburant sauf crash
            if (etat != EtatAvion::ECRASE) {
             dessinerBarreCarburant(fenetre, pos, avion->obtenirPourcentageCarburant());
  }

        // id avion
     Text texteId(police);
        texteId.setString(to_string(avion->obtenirId()));
       texteId.setCharacterSize(14);
       texteId.setFillColor(Color::White);
texteId.setOutlineColor(Color::Black);
          texteId.setOutlineThickness(1);
    
          FloatRect bornesTexte = texteId.getLocalBounds();
    texteId.setOrigin(Vector2f(
   bornesTexte.position.x + bornesTexte.size.x / 2.0f,
     bornesTexte.position.y + bornesTexte.size.y / 2.0f
             ));
     texteId.setPosition(Vector2f(pos.x, pos.y));
 fenetre.draw(texteId);
    }
     }
            
 // supprimer avions partis ou inactifs sauf crashes pour liberer memoire
      avions.erase(
     remove_if(avions.begin(), avions.end(),
          [](const unique_ptr<Avion>& a) { 
      // supprimer si parti ou inactif mais garder crashes pour affichage
EtatAvion etat = a->obtenirEtat();
  return etat == EtatAvion::PARTI || (!a->estActif() && etat != EtatAvion::ECRASE); 
     }),
         avions.end()
     );
            
   /* dessiner legende
   dessinerLegende(fenetre, police);
     
            // dessiner statistiques
    dessinerStatistiques(fenetre, police, tour, avionsActifs, pisteFermee);
     
            // dessiner controles
     dessinerControles(fenetre, police);
    */
         fenetre.display();
   }
        
 // arreter proprement tous avions
 Journaliseur::obtenirInstance().journaliser("Main", "arret avions", "INFO");
        for (auto& avion : avions) {
      avion->arreter();
      }
        
     for (auto& avion : avions) {
            avion->rejoindre();
        }
   
        Journaliseur::obtenirInstance().journaliser("Main", "simulation terminee", "INFO");
        Journaliseur::obtenirInstance().fermer();
  
    } catch (const exception& e) {
        cerr << "erreur " << e.what() << endl;
        Journaliseur::obtenirInstance().journaliser("Main", string("exception ") + e.what(), "ERROR");
        Journaliseur::obtenirInstance().fermer();
        return EXIT_FAILURE;
    }
  
    return EXIT_SUCCESS;
}
