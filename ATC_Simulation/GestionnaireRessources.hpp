#pragma once
#include <SFML/Graphics.hpp>
#include <map>
#include <string>
#include <memory>
#include <iostream>

using namespace std;
using namespace sf;

// gestionnaire ressources singleton optimiser textures
class GestionnaireRessources {
public:
    // instance unique
    static GestionnaireRessources& obtenirInstance() {
        static GestionnaireRessources instance;
  return instance;
    }

    // pas de copie
    GestionnaireRessources(const GestionnaireRessources&) = delete;
    GestionnaireRessources& operator=(const GestionnaireRessources&) = delete;

    // charger texture ou recuperer si deja en cache
    const Texture* chargerTexture(const string& nomFichier) {
// verif deja en cache
        auto it = textures_.find(nomFichier);
     if (it != textures_.end()) {
  return it->second.get();
     }

   // charger nouvelle texture
        auto texture = make_unique<Texture>();
        if (!texture->loadFromFile(nomFichier)) {
   cerr << "[GestionnaireRessources] impossible charger " << nomFichier << endl;
       return nullptr;
        }

        cout << "[GestionnaireRessources] texture chargee " << nomFichier << endl;
        
        // stocker dans cache
        const Texture* ptr = texture.get();
        textures_[nomFichier] = move(texture);
return ptr;
    }

    // verifier si texture existe
    bool possederTexture(const string& nomFichier) const {
  return textures_.find(nomFichier) != textures_.end();
    }

 // nettoyer ressources
    void nettoyer() {
        textures_.clear();
        cout << "[GestionnaireRessources] ressources liberees" << endl;
    }

private:
  GestionnaireRessources() {
        cout << "[GestionnaireRessources] gestionnaire initialise" << endl;
    }

    ~GestionnaireRessources() {
    nettoyer();
    }

    map<string, unique_ptr<Texture>> textures_;
};
