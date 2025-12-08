#pragma once
#include <cmath>
#include <string>

using namespace std;

// position 2d
struct Position {
    float x;
    float y;
    
    Position() : x(0.0f), y(0.0f) {}
    Position(float _x, float _y) : x(_x), y(_y) {}
    
    // calcul distance
    float distanceVers(const Position& autre) const {
 float dx = x - autre.x;
        float dy = y - autre.y;
        return sqrt(dx * dx + dy * dy);
    }
    
    // calcul angle en radians
    float angleVers(const Position& autre) const {
   float dx = autre.x - x;
    float dy = autre.y - y;
        return atan2(dy, dx);
    }
};

// etats avion
enum class EtatAvion {
    APPROCHE,
    ATTENTE,
    ATTERRISSAGE,
    ATTENTE_CROISEMENT,  // nouvel etat : attend de pouvoir traverser la piste droite
    CROISEMENT,     // nouvel etat : traverse vers le parking
    ROULAGE_ENTREE,
    STATIONNE,
    ROULAGE_SORTIE,
    DECOLLAGE,
DEPART,
    URGENCE,
 ECRASE,
    PARTI
};

// conversion etat en texte
inline const char* EtatVersTexte(EtatAvion etat) {
    switch (etat) {
     case EtatAvion::APPROCHE:             return "APPROCHE";
  case EtatAvion::ATTENTE:   return "ATTENTE";
  case EtatAvion::ATTERRISSAGE:         return "ATTERRISSAGE";
case EtatAvion::ATTENTE_CROISEMENT:   return "ATTENTE_CROISEMENT";
     case EtatAvion::CROISEMENT:      return "CROISEMENT";
  case EtatAvion::ROULAGE_ENTREE:       return "ROULAGE_ENTREE";
  case EtatAvion::STATIONNE:return "STATIONNE";
     case EtatAvion::ROULAGE_SORTIE:       return "ROULAGE_SORTIE";
    case EtatAvion::DECOLLAGE:     return "DECOLLAGE";
        case EtatAvion::DEPART:     return "DEPART";
        case EtatAvion::URGENCE:  return "URGENCE";
     case EtatAvion::ECRASE:               return "ECRASE";
  case EtatAvion::PARTI:    return "PARTI";
   default:           return "INCONNU";
 }
}

// config carburant
namespace ConfigCarburant {
    const float CARBURANT_MAX = 100.0f;
    const float TAUX_CONSOMMATION_CARBURANT = 0.5f;
    const float SEUIL_URGENCE_CARBURANT = 20.0f;
    const float TAUX_REMPLISSAGE_CARBURANT = 2.0f;
}
