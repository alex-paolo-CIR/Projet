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
    , vitesseBase_(vitesse)
    , angleAttente_(0.0f)
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
   
        // machine a etats
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
            case EtatAvion::ATTENTE_CROISEMENT:
                phaseAttenteCroisement();
  break;
            case EtatAvion::CROISEMENT:
     phaseCroisement();
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
        // demande piste GAUCHE pour atterrissage
        bool estUrgence = carburant_.load() < ConfigCarburant::SEUIL_URGENCE_CARBURANT;
        if (tour_->demanderPisteGauche(id_, estUrgence)) {
            // transition immediate vers atterrissage
    mettreAJourEtat(EtatAvion::ATTERRISSAGE);
   positionCible_ = tour_->obtenirSeuilPisteGauche();
      aDemandePiste_ = true;
        } else {
            // attente immediate - faire tours en ovale
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
    
    // faire ovale sur le cote GAUCHE au dessus des pistes verticales
    Position centreOvale(200.0f, 200.0f);  // a gauche et en haut
    float rayonX = 100.0f;  // ovale horizontal
    float rayonY = 120.0f;   // ovale vertical

    // augmenter angle pour tourner 3 degres par frame
  angleAttente_ += 3.0f;
    if (angleAttente_ >= 360.0f) {
        angleAttente_ -= 360.0f;
    }
    
    // calculer position sur ovale
    float angleRad = angleAttente_ * 3.14159f / 180.0f;
    Position nouvellePos;
    nouvellePos.x = centreOvale.x + cos(angleRad) * rayonX;
    nouvellePos.y = centreOvale.y + sin(angleRad) * rayonY;
    
    // deplacer vers cette position
    positionCible_ = nouvellePos;
    deplacerVers(positionCible_, TEMPS_TRAME);

    // verifier piste GAUCHE disponible tous les tours complets
    if (angleAttente_ < 5.0f) {
     if (!aDemandePiste_) {
  bool estUrgence = carburant_.load() < ConfigCarburant::SEUIL_URGENCE_CARBURANT;
          if (tour_->demanderPisteGauche(id_, estUrgence)) {
          // piste accordee transition vers atterrissage
      mettreAJourEtat(EtatAvion::ATTERRISSAGE);
                positionCible_ = tour_->obtenirSeuilPisteGauche();
  aDemandePiste_ = true;
         vitesse_ = vitesseBase_ * 1.2f;
            }
        } else {
       // redemander si deja demande
        bool estUrgence = carburant_.load() < ConfigCarburant::SEUIL_URGENCE_CARBURANT;
if (tour_->demanderPisteGauche(id_, estUrgence)) {
  mettreAJourEtat(EtatAvion::ATTERRISSAGE);
       positionCible_ = tour_->obtenirSeuilPisteGauche();
     aDemandePiste_ = true;
     vitesse_ = vitesseBase_ * 1.2f;
 }
        }
    }
}

void Avion::phaseUrgence() {
    // ovale plus serre et plus rapide en urgence sur le cote gauche
 Position centreOvale(200.0f, 200.0f);
 float rayonX = 80.0f;  // plus serre
    float rayonY = 100.0f;
    
    // tourner plus vite en urgence
    angleAttente_ += 4.0f;
    if (angleAttente_ >= 360.0f) {
        angleAttente_ -= 360.0f;
    }
    
    // calculer position sur ovale
    float angleRad = angleAttente_ * 3.14159f / 180.0f;
    Position nouvellePos;
    nouvellePos.x = centreOvale.x + cos(angleRad) * rayonX;
    nouvellePos.y = centreOvale.y + sin(angleRad) * rayonY;
    
    // deplacer vers cette position
    positionCible_ = nouvellePos;
    deplacerVers(positionCible_, TEMPS_TRAME);
    
 // verifier piste GAUCHE disponible tous les demi tours
    if (angleAttente_ < 5.0f || (angleAttente_ > 175.0f && angleAttente_ < 185.0f)) {
  if (!aDemandePiste_) {
   if (tour_->demanderPisteGauche(id_, true)) {
     mettreAJourEtat(EtatAvion::ATTERRISSAGE);
      positionCible_ = tour_->obtenirSeuilPisteGauche();
      aDemandePiste_ = true;
        vitesse_ = vitesseBase_ * 1.5f;
            }
} else {
       if (tour_->demanderPisteGauche(id_, true)) {
   mettreAJourEtat(EtatAvion::ATTERRISSAGE);
          positionCible_ = tour_->obtenirSeuilPisteGauche();
     aDemandePiste_ = true;
        vitesse_ = vitesseBase_ * 1.5f;
   }
        }
    }
}

void Avion::phaseAtterrissage() {
    // augmenter vitesse sur piste gauche pour degager vite
    vitesse_ = vitesseBase_ * 2.0f;
    
    deplacerVers(positionCible_, TEMPS_TRAME);
    
 if (aCibleAtteinte(positionCible_)) {
        // atteint seuil piste gauche
        // continuer jusqu au bout de la piste gauche
        positionCible_ = tour_->obtenirFinPisteGauche();
    }
    
    if (aCibleAtteinte(tour_->obtenirFinPisteGauche(), 10.0f)) {
      // au bout de la piste gauche
        // POINT CRITIQUE : doit traverser vers parking
    // mais doit verifier piste DROITE libre
      
        // liberer piste gauche maintenant
        if (aDemandePiste_) {
          tour_->libererPisteGauche(id_);
            aDemandePiste_ = false;
   }
        
  // passer en attente croisement
 mettreAJourEtat(EtatAvion::ATTENTE_CROISEMENT);
    }
}

void Avion::phaseAttenteCroisement() {
    // avion ARRETE au bout de la piste gauche
    // attend que la piste DROITE soit libre pour traverser
    vitesse_ = 0.0f;  // ARRETER NET
    
    // verifier si piste droite libre
    if (!tour_->pisteDroiteOccupee()) {
 // piste droite libre! demander autorisation
  if (tour_->demanderPisteDroite(id_)) {
            // autorisation accordee transition vers croisement
            mettreAJourEtat(EtatAvion::CROISEMENT);
     positionCible_ = tour_->obtenirPointCroisement();
   vitesse_ = vitesseBase_ * 1.0f;  // vitesse normale pour traverser
            aDemandePiste_ = true;
        
            stringstream ss;
 ss << "avion " << id_ << " traverse vers parking (piste droite libre)";
    Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
        }
    }
    // sinon reste immobile et attend
}

void Avion::phaseCroisement() {
    // traverse la piste droite vers le parking
    deplacerVers(positionCible_, TEMPS_TRAME);
    
    if (aCibleAtteinte(positionCible_)) {
      // croisement termine liberer piste droite
        if (aDemandePiste_) {
tour_->libererPisteDroite(id_);
 aDemandePiste_ = false;
        }
        
        // demander parking
        parkingAssigne_ = tour_->assignerParking(id_);
        
        if (parkingAssigne_ >= 0) {
            mettreAJourEtat(EtatAvion::ROULAGE_ENTREE);
   positionCible_ = tour_->obtenirPositionParking(parkingAssigne_);
   vitesse_ = vitesseBase_ * 1.5f;
        } else {
            // pas de parking disponible aller directement au point depart
    mettreAJourEtat(EtatAvion::ROULAGE_SORTIE);
        positionCible_ = tour_->obtenirSeuilPisteDroite();
        }
 }
}

void Avion::phaseRoulageEntree() {
    // vitesse acceleree pour rouler vers parking
    vitesse_ = vitesseBase_ * 1.5f;
    deplacerVers(positionCible_, TEMPS_TRAME);
    
    if (aCibleAtteinte(positionCible_)) {
        mettreAJourEtat(EtatAvion::STATIONNE);
        
    stringstream ss;
        ss << "avion " << id_ << " arrive parking ravitaillement carburant " << carburant_.load();
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
    }
}

void Avion::phaseStationnement() {
    // ravitaillement progressif accelere
    ravitaillerAuParking(TEMPS_TRAME * 2.0f);
    
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
      
 // *** NOUVELLE LOGIQUE : Verifier si piste droite occupee ***
        // Si un avion decolle, attendre qu'il libere la piste
  if (tour_->pisteDroiteOccupee()) {
            // Un autre avion decolle, rester au parking
            stringstream ss;
            ss << "avion " << id_ << " pret a partir mais attend fin decollage autre avion";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
            return;  // Attendre, ne pas quitter le parking
        }
     
        // Piste libre, on peut partir
        stringstream ss;
        ss << "avion " << id_ << " ravitaille " << carburant_.load() 
           << " apres " << ecoule << " sec - pret au depart";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
        
        // liberer parking
        tour_->libererParking(id_, parkingAssigne_);
        
      // retour seuil piste DROITE
        mettreAJourEtat(EtatAvion::ROULAGE_SORTIE);
     positionCible_ = tour_->obtenirSeuilPisteDroite();
        
      // nettoyer timer
        minuteursParking.erase(id_);
aDemandePiste_ = false;
    }
}

void Avion::phaseRoulageSortie() {
    // vitesse acceleree pour rouler vers piste droite
    vitesse_ = vitesseBase_ * 1.5f;
    deplacerVers(positionCible_, TEMPS_TRAME);
    
  if (aCibleAtteinte(positionCible_)) {
        // arrive au seuil piste DROITE pour decoller
        if (!aDemandePiste_) {
            if (tour_->demanderPisteDroite(id_)) {
      // autorisation decollage
       mettreAJourEtat(EtatAvion::DECOLLAGE);
       positionCible_ = tour_->obtenirPointDepart();
        aDemandePiste_ = true;
     }
        }
    }
}

void Avion::phaseDecollage() {
    // augmenter vitesse sur piste DROITE pour degager vite
    vitesse_ = vitesseBase_ * 2.0f;
    
    deplacerVers(positionCible_, TEMPS_TRAME);
    
 if (aCibleAtteinte(positionCible_)) {
        // decollage termine liberer piste DROITE immediatement
        tour_->libererPisteDroite(id_);
        aDemandePiste_ = false;
        
        // reinitialiser vitesse normale
        vitesse_ = vitesseBase_;
        
        mettreAJourEtat(EtatAvion::DEPART);
    }
}

void Avion::phaseDepart() {
    deplacerVers(positionCible_, TEMPS_TRAME);
    
    Position posActuelle = obtenirPosition();
    // augmenter les limites pour que avions restent visibles plus longtemps
    // seulement quand vraiment loin de lecran
    if (posActuelle.y > 750.0f || posActuelle.y < -150.0f || 
  posActuelle.x > 950.0f || posActuelle.x < -150.0f) {
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
