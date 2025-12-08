#include "Avion.hpp"
#include <sstream>
#include <cmath>
#include <map>

using namespace std;

// definition constantes
const float Avion::TEMPS_TRAME = 0.016f;
const float Avion::TEMPS_PARKING = 8.0f;
const float Avion::INTERVALLE_VERIFICATION_ATTENTE = 0.5f;

Avion::Avion(int id, TourControle* tour, Position posDepart, float vitesse)
    : id_(id)
    , tour_(tour)
    , position_(posDepart)
    , positionCible_(posDepart)
    , angleRotation_(0.0f)
    , vitesse_(vitesse)
    , etat_(EtatAvion::APPROCHE)
    , actif_(true)
    , enCours_(false)
    , carburant_(ConfigCarburant::CARBURANT_MAX)
    , aDemandePiste_(false)
    , parkingAssigne_(-1)
{
    stringstream ss;
    ss << "avion " << id_ << " cree position (" 
       << position_.x << ", " << position_.y << ") carburant " << carburant_.load();
    Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
}

Avion::~Avion() {
    arreter();
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
}

void Avion::demarrer() {
enCours_ = true;
  thread_ = make_unique<thread>(&Avion::lancer, this);
    
    stringstream ss;
  ss << "avion " << id_ << " thread demarre";
    Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
}

void Avion::arreter() {
    enCours_ = false;
    actif_ = false;
}

void Avion::rejoindre() {
    if (thread_ && thread_->joinable()) {
   thread_->join();
    }
}

Position Avion::obtenirPosition() const {
    lock_guard<mutex> verrou(mutexPosition_);
    return position_;
}

float Avion::obtenirAngleRotation() const {
    lock_guard<mutex> verrou(mutexRotation_);
    return angleRotation_;
}

void Avion::marquerCommeEcrase() {
    mettreAJourEtat(EtatAvion::ECRASE);
    actif_ = false;
}

void Avion::lancer() {
    while (enCours_.load() && actif_.load()) {
 EtatAvion etatActuel = etat_.load();
        
        // maj carburant sauf parking et crash
    if (etatActuel != EtatAvion::STATIONNE && etatActuel != EtatAvion::ECRASE) {
     mettreAJourCarburant(TEMPS_TRAME);
     }
     
        // verif carburant prioritaire
   verifierStatutCarburant();
    
        // si crash arreter immediatement
        if (etat_.load() == EtatAvion::ECRASE) {
     phaseEcrase();
            break;
}
        
   // machine a etats sans sleep inutiles
        switch (etatActuel) {
        case EtatAvion::APPROCHE:
   phaseApproche();
    break;
        case EtatAvion::ATTENTE:
    phaseAttente();
            break;
        case EtatAvion::ATTERRISSAGE:
        phaseAtterrissage();
      break;
        case EtatAvion::ROULAGE_ENTREE:
       phaseRoulageEntree();
         break;
  case EtatAvion::STATIONNE:
            phaseStationnement();
            break;
     case EtatAvion::ROULAGE_SORTIE:
      phaseRoulageSortie();
      break;
        case EtatAvion::DECOLLAGE:
            phaseDecollage();
   break;
        case EtatAvion::DEPART:
            phaseDepart();
break;
  case EtatAvion::URGENCE:
       phaseUrgence();
            break;
     case EtatAvion::PARTI:
            actif_ = false;
 break;
        default:
    break;
      }
        
        // seul sleep boucle physique 60 fps
        this_thread::sleep_for(chrono::milliseconds((int)(TEMPS_TRAME * 1000)));
    }
    
    stringstream ss;
    ss << "avion " << id_ << " thread termine";
    Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
}

void Avion::mettreAJourCarburant(float dt) {
    float carburantActuel = carburant_.load();
    carburantActuel -= ConfigCarburant::TAUX_CONSOMMATION_CARBURANT * dt;
    
    if (carburantActuel < 0.0f) {
     carburantActuel = 0.0f;
    }
    
    carburant_ = carburantActuel;
}

void Avion::ravitaillerAuParking(float dt) {
 float carburantActuel = carburant_.load();
    carburantActuel += ConfigCarburant::TAUX_REMPLISSAGE_CARBURANT * dt;
    
    if (carburantActuel > ConfigCarburant::CARBURANT_MAX) {
        carburantActuel = ConfigCarburant::CARBURANT_MAX;
    }
    
    carburant_ = carburantActuel;
}

void Avion::verifierStatutCarburant() {
    float carburantActuel = carburant_.load();
    EtatAvion etatActuel = etat_.load();
    
    // crash immediat si plus de carburant
    if (carburantActuel <= 0.0f && etatActuel != EtatAvion::ECRASE) {
  stringstream ss;
        ss << "avion " << id_ << " crash plus de carburant position (" 
           << position_.x << ", " << position_.y << ")";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "ERROR");
 marquerCommeEcrase();
   return;
    }
    
    // urgence si carburant bas
    if (carburantActuel < ConfigCarburant::SEUIL_URGENCE_CARBURANT && 
        etatActuel != EtatAvion::URGENCE &&
        etatActuel != EtatAvion::ECRASE &&
        etatActuel != EtatAvion::STATIONNE) {
  
        stringstream ss;
    ss << "avion " << id_ << " urgence carburant bas " << carburantActuel;
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "WARN");
 mettreAJourEtat(EtatAvion::URGENCE);
    }
}

void Avion::phaseApproche() {
    positionCible_ = tour_->obtenirPointApproche();
deplacerVers(positionCible_, TEMPS_TRAME);
  
    if (aCibleAtteinte(positionCible_)) {
        // demande piste
 bool estUrgence = carburant_.load() < ConfigCarburant::SEUIL_URGENCE_CARBURANT;
        if (tour_->demanderPiste(id_, estUrgence)) {
          // transition immediate vers atterrissage
            mettreAJourEtat(EtatAvion::ATTERRISSAGE);
    positionCible_ = tour_->obtenirSeuilPiste();
            aDemandePiste_ = true;
        } else {
     // attente immediate
  mettreAJourEtat(EtatAvion::ATTENTE);
            aDemandePiste_ = false;
        }
    }
}

void Avion::phaseAttente() {
    // verif si passage urgence
    if (carburant_.load() < ConfigCarburant::SEUIL_URGENCE_CARBURANT && 
        etat_.load() != EtatAvion::URGENCE) {
        mettreAJourEtat(EtatAvion::URGENCE);
        aDemandePiste_ = false;
        return;
    }
    
    // redemander piste seulement si pas deja demande
    if (!aDemandePiste_) {
        bool estUrgence = carburant_.load() < ConfigCarburant::SEUIL_URGENCE_CARBURANT;
        if (tour_->demanderPiste(id_, estUrgence)) {
  // transition immediate vers atterrissage
            mettreAJourEtat(EtatAvion::ATTERRISSAGE);
positionCible_ = tour_->obtenirSeuilPiste();
 aDemandePiste_ = true;
      } else {
         // attendre un peu avant de redemander pour ne pas spammer
            this_thread::sleep_for(chrono::milliseconds(500));
        }
    } else {
    // si deja demande verifier periodiquement
        this_thread::sleep_for(chrono::milliseconds(500));
 
      // essayer a nouveau apres attente
        bool estUrgence = carburant_.load() < ConfigCarburant::SEUIL_URGENCE_CARBURANT;
   if (tour_->demanderPiste(id_, estUrgence)) {
    mettreAJourEtat(EtatAvion::ATTERRISSAGE);
    positionCible_ = tour_->obtenirSeuilPiste();
         aDemandePiste_ = true;
        }
    }
}

void Avion::phaseUrgence() {
    // redemander piste seulement si pas deja demande
    if (!aDemandePiste_) {
      if (tour_->demanderPiste(id_, true)) {
            // transition immediate vers atterrissage
          mettreAJourEtat(EtatAvion::ATTERRISSAGE);
            positionCible_ = tour_->obtenirSeuilPiste();
     aDemandePiste_ = true;
      } else {
            // attendre un peu avant de redemander
          this_thread::sleep_for(chrono::milliseconds(500));
        }
    } else {
 // si deja demande verifier periodiquement
        this_thread::sleep_for(chrono::milliseconds(500));
    
     // essayer a nouveau apres attente
        if (tour_->demanderPiste(id_, true)) {
     mettreAJourEtat(EtatAvion::ATTERRISSAGE);
     positionCible_ = tour_->obtenirSeuilPiste();
            aDemandePiste_ = true;
        }
    }
}

void Avion::phaseAtterrissage() {
  deplacerVers(positionCible_, TEMPS_TRAME);
    
    if (aCibleAtteinte(positionCible_)) {
        // atterrissage termine liberer piste
        tour_->libererPiste(id_);
   aDemandePiste_ = false;
        
        // demander parking
        parkingAssigne_ = tour_->assignerParking(id_);
        
      if (parkingAssigne_ >= 0) {
            mettreAJourEtat(EtatAvion::ROULAGE_ENTREE);
            positionCible_ = tour_->obtenirPositionParking(parkingAssigne_);
        } else {
            // pas de parking
   mettreAJourEtat(EtatAvion::DEPART);
            positionCible_ = tour_->obtenirPointDepart();
        }
    }
}

void Avion::phaseRoulageEntree() {
    deplacerVers(positionCible_, TEMPS_TRAME * 0.5f);
    
    if (aCibleAtteinte(positionCible_)) {
mettreAJourEtat(EtatAvion::STATIONNE);
        
        stringstream ss;
        ss << "avion " << id_ << " arrive parking ravitaillement carburant " << carburant_.load();
   Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
    }
}

void Avion::phaseStationnement() {
    // ravitaillement progressif
    ravitaillerAuParking(TEMPS_TRAME);

 // timer local temps parking
    static map<int, chrono::steady_clock::time_point> minuteursParking;
    
 // init timer si necessaire
    if (minuteursParking.find(id_) == minuteursParking.end()) {
        minuteursParking[id_] = chrono::steady_clock::now();
    }
    
    long long ecoule = chrono::duration_cast<chrono::seconds>(
        chrono::steady_clock::now() - minuteursParking[id_]
    ).count();
    
    // condition carburant >= 95% et temps >= 8 sec
    if (carburant_.load() >= ConfigCarburant::CARBURANT_MAX * 0.95f && ecoule >= (long long)TEMPS_PARKING) {
        stringstream ss;
      ss << "avion " << id_ << " ravitaille " << carburant_.load() 
         << " apres " << ecoule << " sec";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
        
        // liberer parking
        tour_->libererParking(id_, parkingAssigne_);
        
        // retour seuil piste
    mettreAJourEtat(EtatAvion::ROULAGE_SORTIE);
    positionCible_ = tour_->obtenirSeuilPiste();
      
        // nettoyer timer
        minuteursParking.erase(id_);
        aDemandePiste_ = false;
    }
}

void Avion::phaseRoulageSortie() {
    deplacerVers(positionCible_, TEMPS_TRAME * 0.5f);
    
 if (aCibleAtteinte(positionCible_)) {
      // demander piste pour decoller pas de sleep
        if (!aDemandePiste_) {
    if (tour_->demanderPiste(id_, false)) {
            // transition immediate vers decollage
    mettreAJourEtat(EtatAvion::DECOLLAGE);
          positionCible_ = tour_->obtenirPointDepart();
            aDemandePiste_ = true;
            }
        }
    }
}

void Avion::phaseDecollage() {
    deplacerVers(positionCible_, TEMPS_TRAME);
    
    if (aCibleAtteinte(positionCible_)) {
        // decollage termine liberer piste
        tour_->libererPiste(id_);
        aDemandePiste_ = false;
     mettreAJourEtat(EtatAvion::DEPART);
    }
}

void Avion::phaseDepart() {
    deplacerVers(positionCible_, TEMPS_TRAME);
    
    Position posActuelle = obtenirPosition();
    if (posActuelle.y > 650.0f || posActuelle.y < -50.0f || 
        posActuelle.x > 850.0f || posActuelle.x < -50.0f) {
        mettreAJourEtat(EtatAvion::PARTI);
    }
}

void Avion::phaseEcrase() {
    // avion reste immobile
    stringstream ss;
    ss << "avion " << id_ << " reste ecrase position (" 
       << position_.x << ", " << position_.y << ") bloque piste";
    Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "ERROR");
    
    // rester actif pour affichage mais arreter thread
    enCours_ = false;
}

void Avion::deplacerVers(const Position& cible, float dt) {
    // ne pas bouger si crash
    if (etat_.load() == EtatAvion::ECRASE) {
  return;
    }

    lock_guard<mutex> verrou(mutexPosition_);
 
    float dx = cible.x - position_.x;
    float dy = cible.y - position_.y;
    float distance = sqrt(dx * dx + dy * dy);
    
    if (distance > 0.1f) {
        float distanceDeplacement = vitesse_ * dt;
      if (distanceDeplacement > distance) {
            distanceDeplacement = distance;
        }
        
        position_.x += (dx / distance) * distanceDeplacement;
        position_.y += (dy / distance) * distanceDeplacement;
        
        // maj angle rotation
     {
     lock_guard<mutex> verrouRot(mutexRotation_);
            angleRotation_ = atan2(dy, dx) * 180.0f / 3.14159f;
        }
    }
}

void Avion::mettreAJourEtat(EtatAvion nouvelEtat) {
    EtatAvion ancienEtat = etat_.load();
    etat_ = nouvelEtat;
    
    stringstream ss;
    ss << "avion " << id_ << " changement etat " 
       << EtatVersTexte(ancienEtat) << " vers " << EtatVersTexte(nouvelEtat);
    Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
}

bool Avion::aCibleAtteinte(const Position& cible, float seuil) const {
    Position posActuelle = obtenirPosition();
    return posActuelle.distanceVers(cible) < seuil;
}
