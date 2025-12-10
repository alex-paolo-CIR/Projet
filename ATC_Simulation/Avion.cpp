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
    ss << "avion " << id_ << " thread terme";
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
    bool doitCreerTrajectoire = false;

    {
        lock_guard<mutex> verrou(mutexWaypoints_);
        if (waypoints_.empty()) {
            doitCreerTrajectoire = true;
        }
    }

    if (doitCreerTrajectoire) {
        Position depart = obtenirPosition();
        Position seuilPiste = tour_->obtenirSeuilPisteGauche();
        
        // creer une approche naturelle en 2 temps:
        // 1. aller vers un point d'alignement (meme x que la piste, y au dessus)
        // 2. descendre droit vers le seuil
        
        Position pointAlignement(seuilPiste.x, depart.y);  // s'aligner en x avec la piste
        
        // si on n'est pas deja aligne, ajouter le point d'alignement
        if (abs(depart.x - seuilPiste.x) > 20.0f) {
            ajouterWaypoint(pointAlignement);
        }
        
        // ajouter le seuil de piste
        ajouterWaypoint(seuilPiste);

        stringstream ss;
        ss << "avion " << id_ << " approche alignee vers piste x=" << seuilPiste.x;
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
    }

    mettreAJourWaypoints(TEMPS_TRAME);

    Position seuilPiste = tour_->obtenirSeuilPisteGauche();
    if (aCibleAtteinte(seuilPiste, 15.0f)) {
        viderWaypoints();

        bool estUrgence = carburant_.load() < ConfigCarburant::SEUIL_URGENCE_CARBURANT;
        if (tour_->demanderPisteGauche(id_, estUrgence)) {
            mettreAJourEtat(EtatAvion::ATTERRISSAGE);
            aDemandePiste_ = true;

            // deja aligne, aller droit vers fin de piste
            Position posActuelle = obtenirPosition();
            Position finPiste = tour_->obtenirFinPisteGauche();
            ajouterWaypoint(finPiste);
        }
        else {
            mettreAJourEtat(EtatAvion::ATTENTE);
            aDemandePiste_ = false;
        }
    }
}

void Avion::phaseAttente() {
    if (carburant_.load() < ConfigCarburant::SEUIL_URGENCE_CARBURANT && 
        etat_.load() != EtatAvion::URGENCE) {
        mettreAJourEtat(EtatAvion::URGENCE);
        aDemandePiste_ = false;
        return;
    }
    
    // demander autorisation a chaque frame
    bool estUrgence = carburant_.load() < ConfigCarburant::SEUIL_URGENCE_CARBURANT;
    if (tour_->demanderPisteGauche(id_, estUrgence)) {
        // autorisation recue, aller atterrir immediatement
        viderWaypoints();
        mettreAJourEtat(EtatAvion::ATTERRISSAGE);
        aDemandePiste_ = true;
        vitesse_ = vitesseBase_ * 1.2f;
        
        Position seuilPiste = tour_->obtenirSeuilPisteGauche();
        ajouterWaypoint(seuilPiste);
        ajouterWaypoint(tour_->obtenirFinPisteGauche());
        
        stringstream ss;
        ss << "avion " << id_ << " quitte attente, atterrissage autorise";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
        return;
    }
    
    // pattern d'attente simple : cercle a gauche de la piste
    static map<int, int> etapeAttente;
    
    if (etapeAttente.find(id_) == etapeAttente.end()) {
        etapeAttente[id_] = 0;
    }
    
    Position seuilPiste = tour_->obtenirSeuilPisteGauche();
    float xPiste = seuilPiste.x;
    
    Position coins[4] = {
        Position(xPiste - 120.0f, 80.0f),
        Position(xPiste - 120.0f, 180.0f),
        Position(xPiste - 40.0f, 180.0f),
        Position(xPiste - 40.0f, 80.0f)
    };
    
    int etape = etapeAttente[id_];
    Position cible = coins[etape];
    
    {
        lock_guard<mutex> verrou(mutexWaypoints_);
        if (waypoints_.empty()) {
            mutexWaypoints_.unlock();
            ajouterWaypoint(cible);
            mutexWaypoints_.lock();
        }
    }
    
    mettreAJourWaypoints(TEMPS_TRAME);
    
    if (aCibleAtteinte(cible, 10.0f)) {
        etapeAttente[id_] = (etape + 1) % 4;
        viderWaypoints();
        ajouterWaypoint(coins[etapeAttente[id_]]);
    }
}

void Avion::phaseUrgence() {
    // demander autorisation a chaque frame (priorite urgence)
    if (tour_->demanderPisteGauche(id_, true)) {
        // autorisation recue, aller atterrir immediatement
        viderWaypoints();
        mettreAJourEtat(EtatAvion::ATTERRISSAGE);
        aDemandePiste_ = true;
        vitesse_ = vitesseBase_ * 1.5f;
        
        Position seuilPiste = tour_->obtenirSeuilPisteGauche();
        ajouterWaypoint(seuilPiste);
        ajouterWaypoint(tour_->obtenirFinPisteGauche());
        
        stringstream ss;
        ss << "avion " << id_ << " urgence terminee, atterrissage prioritaire";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "WARN");
        return;
    }
    
    // pattern d'attente urgence (plus serre)
    static map<int, int> etapeUrgence;
    
    if (etapeUrgence.find(id_) == etapeUrgence.end()) {
        etapeUrgence[id_] = 0;
    }
    
    Position seuilPiste = tour_->obtenirSeuilPisteGauche();
    float xPiste = seuilPiste.x;
    
    Position coins[4] = {
        Position(xPiste - 80.0f, 60.0f),
        Position(xPiste - 80.0f, 140.0f),
        Position(xPiste - 20.0f, 140.0f),
        Position(xPiste - 20.0f, 60.0f)
    };
    
    vitesse_ = vitesseBase_ * 1.3f;
    
    int etape = etapeUrgence[id_];
    Position cible = coins[etape];
    
    {
        lock_guard<mutex> verrou(mutexWaypoints_);
        if (waypoints_.empty()) {
            mutexWaypoints_.unlock();
            ajouterWaypoint(cible);
            mutexWaypoints_.lock();
        }
    }
    
    mettreAJourWaypoints(TEMPS_TRAME);
    
    if (aCibleAtteinte(cible, 10.0f)) {
        etapeUrgence[id_] = (etape + 1) % 4;
        viderWaypoints();
        ajouterWaypoint(coins[etapeUrgence[id_]]);
    }
}

void Avion::phaseAtterrissage() {
    // Augmenter vitesse sur piste
    vitesse_ = vitesseBase_ * 2.0f;

    
    {
        lock_guard<mutex> verrou(mutexWaypoints_);
        if (waypoints_.empty()) {
            Position depart = obtenirPosition();
            Position arrivee = tour_->obtenirFinPisteGauche();

            // Libérer mutex avant d'appeler ajouterTrajectoire (qui le verrouille)
            mutexWaypoints_.unlock();
            ajouterTrajectoire(depart, arrivee);
            mutexWaypoints_.lock();

            stringstream ss;
            ss << "avion " << id_ << " atterrissage rectiligne de ("
                << depart.x << "," << depart.y << ") vers ("
                << arrivee.x << "," << arrivee.y << ")";
            Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
        }
    }

    // Suivre waypoints définis
    mettreAJourWaypoints(TEMPS_TRAME);

    Position finPiste = tour_->obtenirFinPisteGauche();

    // Si arrivé au bout de la piste
    if (aCibleAtteinte(finPiste, 10.0f)) {
        viderWaypoints();

        // Libérer piste gauche
        if (aDemandePiste_) {
            tour_->libererPisteGauche(id_);
            aDemandePiste_ = false;

            stringstream ss;
            ss << "avion " << id_ << " a termine atterrissage, libere piste gauche";
            Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
        }

        // nouvelle logique de degagement
        // avancer de la taille de l'avion (40.0f)
        Position posAvancee = finPiste;
        posAvancee.y += 40.0f;
        ajouterWaypoint(posAvancee);

        // assigner parking
        parkingAssigne_ = tour_->assignerParking(id_);

        if (parkingAssigne_ >= 0) {
            // aller vers le parking en passant par la gauche de la lignee (x=600)
            Position posCroisementBas(600.0f, posAvancee.y); // aligne avec taxiway gauche en x, mais en bas
            ajouterWaypoint(posCroisementBas);
            
            Position posParking = tour_->obtenirPositionParking(parkingAssigne_);
            // point d'entree taxiway en face du parking
            Position posEntreeParking(600.0f, posParking.y);
            ajouterWaypoint(posEntreeParking);
            
            // entrer dans le parking
            ajouterWaypoint(posParking);
            
            mettreAJourEtat(EtatAvion::ROULAGE_ENTREE);
            
            stringstream ss;
            ss << "avion " << id_ << " degage vers parking " << parkingAssigne_ << " via taxiway gauche";
            Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
        } else {
            // pas de parking, aller vers sortie (piste droite)
            Position posCroisementBas(450.0f, posAvancee.y); // aligne avec piste droite
            ajouterWaypoint(posCroisementBas);
            
            Position seuilPiste = tour_->obtenirSeuilPisteDroite();
            ajouterWaypoint(seuilPiste);
            
            mettreAJourEtat(EtatAvion::ROULAGE_SORTIE);
            
            stringstream ss;
            ss << "avion " << id_ << " pas de parking, degage vers piste droite";
            Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "WARN");
        }
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
    // Définir trajectoire RECTILIGNE une seule fois
    bool doitCreerTrajectoire = false;

    {
        lock_guard<mutex> verrou(mutexWaypoints_);
        if (waypoints_.empty()) {
            doitCreerTrajectoire = true;
        }
    }

    if (doitCreerTrajectoire) {
        Position depart = obtenirPosition();
        Position arrivee = tour_->obtenirPointCroisement();

        ajouterTrajectoire(depart, arrivee);

        stringstream ss;
        ss << "avion " << id_ << " croisement rectiligne de ("
            << depart.x << "," << depart.y << ") vers ("
            << arrivee.x << "," << arrivee.y << ")";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
    }

    // Suivre waypoints
    mettreAJourWaypoints(TEMPS_TRAME);

    Position pointCroisement = tour_->obtenirPointCroisement();
    if (aCibleAtteinte(pointCroisement, 10.0f)) {
        viderWaypoints();

        // Libérer piste droite
        if (aDemandePiste_) {
            tour_->libererPisteDroite(id_);
            aDemandePiste_ = false;
        }

        // Demander parking
        parkingAssigne_ = tour_->assignerParking(id_);

        if (parkingAssigne_ >= 0) {
            mettreAJourEtat(EtatAvion::ROULAGE_ENTREE);
            Position posActuelle = obtenirPosition();
            ajouterTrajectoire(posActuelle, tour_->obtenirPositionParking(parkingAssigne_));
            vitesse_ = vitesseBase_ * 1.5f;
        }
        else {
            mettreAJourEtat(EtatAvion::ROULAGE_SORTIE);
            Position posActuelle = obtenirPosition();
            ajouterTrajectoire(posActuelle, tour_->obtenirSeuilPisteDroite());
        }
    }
}

void Avion::phaseRoulageEntree() {
    // *** CORRECTION : Utiliser système de waypoints ***

    // Définir trajectoire une seule fois
    bool doitCreerTrajectoire = false;

    {
        lock_guard<mutex> verrou(mutexWaypoints_);
        if (waypoints_.empty()) {
            doitCreerTrajectoire = true;
        }
    }

    if (doitCreerTrajectoire && parkingAssigne_ >= 0) {
        Position depart = obtenirPosition();
        Position arrivee = tour_->obtenirPositionParking(parkingAssigne_);

        ajouterTrajectoire(depart, arrivee);

        stringstream ss;
        ss << "avion " << id_ << " roulage vers parking " << parkingAssigne_
            << " de (" << depart.x << "," << depart.y << ") vers ("
            << arrivee.x << "," << arrivee.y << ")";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
    }

    // Vitesse accélérée
    vitesse_ = vitesseBase_ * 1.5f;

    // Suivre waypoints
    mettreAJourWaypoints(TEMPS_TRAME);

    // Vérifier arrivée parking
    if (parkingAssigne_ >= 0) {
        Position posParking = tour_->obtenirPositionParking(parkingAssigne_);
        if (aCibleAtteinte(posParking, 10.0f)) {
            viderWaypoints();
            mettreAJourEtat(EtatAvion::STATIONNE);

            stringstream ss;
            ss << "avion " << id_ << " arrive parking " << parkingAssigne_
                << " ravitaillement carburant " << carburant_.load();
            Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
        }
    }
}

void Avion::phaseStationnement() {
    // ravitaillement progressif accelere
    ravitaillerAuParking(TEMPS_TRAME * 2.0f);
    
    static map<int, chrono::steady_clock::time_point> minuteursParking;
    
    if (minuteursParking.find(id_) == minuteursParking.end()) {
        minuteursParking[id_] = chrono::steady_clock::now();
    }
    
    long long ecoule = chrono::duration_cast<chrono::seconds>(
        chrono::steady_clock::now() - minuteursParking[id_]
    ).count();
    
    if (carburant_.load() >= ConfigCarburant::CARBURANT_MAX * 0.95f && ecoule >= (long long)TEMPS_PARKING) {
  
        // Les avions sortent par le taxiway (x=700) puis vont vers la piste droite
        // Pas besoin de verifier la piste gauche car on ne la traverse pas
        // On verifie seulement si la piste droite n'est pas surchargee
        
        // si la file de decollage est deja longue, on attend
        int rangFile = tour_->obtenirRangDecollage(id_);
        if (rangFile == -1) {
            // on n'est pas encore dans la file, verifier la taille
            // laisser max 3 avions dans la file avant d'en ajouter d'autres
        }
     
        stringstream ss;
        ss << "avion " << id_ << " ravitaille " << carburant_.load() 
           << " apres " << ecoule << " sec - pret au depart";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
        
        tour_->libererParking(id_, parkingAssigne_);
        
        mettreAJourEtat(EtatAvion::ROULAGE_SORTIE);
        
        // trajectoire de sortie optimisee
        // taxiway d'entree est a y = finPiste + 40 = 550 + 40 = 590
        // on sort a y = 630 pour passer en dessous du taxiway d'entree
        Position posActuelle = obtenirPosition();
        Position finPiste = tour_->obtenirFinPisteGauche();
        float yTaxiwayEntree = finPiste.y + 40.0f;  // 590
        float ySortie = yTaxiwayEntree + 40.0f;      // 630 - en dessous du taxiway entree
        
        // 1. sortir vers la droite (x=700)
        Position posSortieParking(700.0f, posActuelle.y);
        ajouterWaypoint(posSortieParking);
        
        // 2. descendre juste en dessous du taxiway d'entree
        Position posContournementBas(700.0f, ySortie);
        ajouterWaypoint(posContournementBas);
        
        // 3. aller vers l'alignement piste droite (x=450)
        Position posAlignementPiste(450.0f, ySortie);
        ajouterWaypoint(posAlignementPiste);
        
        // La suite (file d'attente) est geree par phaseRoulageSortie
        
        minuteursParking.erase(id_);
        aDemandePiste_ = false;
    }
}

void Avion::phaseRoulageSortie() {
    // rejoindre la file d'attente (si pas deja fait)
    tour_->rejoindreFileDecollage(id_);
    
    // obtenir rang dans la file
    int rang = tour_->obtenirRangDecollage(id_);
    
    if (rang == -1) return;
    
    // Vitesse accélérée
    vitesse_ = vitesseBase_ * 1.5f;

    // Verifier si on est aligne avec la piste (x=450)
    Position posActuelle = obtenirPosition();
    bool aligne = abs(posActuelle.x - 450.0f) < 10.0f;

    if (aligne) {
        // on est dans la file d'attente (axe vertical)
        
        // calculer position cible selon rang
        Position seuilPiste = tour_->obtenirSeuilPisteDroite();
        Position cibleAttente = seuilPiste;
        cibleAttente.y += rang * 60.0f;
        
        // verifier si le prochain waypoint correspond a notre cible
        // sinon on met a jour la trajectoire
        bool trajectoireAJour = false;
        {
            lock_guard<mutex> verrou(mutexWaypoints_);
            if (!waypoints_.empty()) {
                Position dernier = waypoints_.back();
                if (abs(dernier.y - cibleAttente.y) < 1.0f) {
                    trajectoireAJour = true;
                }
            }
        }
        
        if (!trajectoireAJour) {
            // redefinir trajectoire vers la cible d'attente
            viderWaypoints();
            ajouterTrajectoire(posActuelle, cibleAttente);
        }
        
        // si on est arrive a la position d'attente
        if (aCibleAtteinte(cibleAttente, 5.0f)) {
            // si on est premier (rang 0), on tente de decoller
            if (rang == 0) {
                if (tour_->accorderDecollage(id_)) {
                    mettreAJourEtat(EtatAvion::DECOLLAGE);
                    aDemandePiste_ = true;
                    
                    // definir trajectoire decollage
                    ajouterTrajectoire(posActuelle, tour_->obtenirPointDepart());
                    
                    stringstream ss;
                    ss << "avion " << id_ << " autorisation decollage accordee";
                    Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
                }
            }
        }
    }
    
    // Suivre waypoints (contournement ou file d'attente)
    mettreAJourWaypoints(TEMPS_TRAME);
}

void Avion::phaseDecollage() {
    

    // Définir trajectoire une seule fois
    bool doitCreerTrajectoire = false;

    {
        lock_guard<mutex> verrou(mutexWaypoints_);
        if (waypoints_.empty()) {
            doitCreerTrajectoire = true;
        }
    }

    if (doitCreerTrajectoire) {
        Position depart = obtenirPosition();
        Position arrivee = tour_->obtenirPointDepart();

        ajouterTrajectoire(depart, arrivee);

        stringstream ss;
        ss << "avion " << id_ << " decollage de ("
            << depart.x << "," << depart.y << ") vers ("
            << arrivee.x << "," << arrivee.y << ")";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
    }

    // Augmenter vitesse sur piste DROITE
    vitesse_ = vitesseBase_ * 2.0f;

    // Suivre waypoints
    mettreAJourWaypoints(TEMPS_TRAME);

    // Vérifier fin décollage
    Position pointDepart = tour_->obtenirPointDepart();
    if (aCibleAtteinte(pointDepart, 10.0f)) {
        viderWaypoints();

        // Libérer piste DROITE immédiatement
        tour_->libererPisteDroite(id_);
        aDemandePiste_ = false;

        // Réinitialiser vitesse normale
        vitesse_ = vitesseBase_;

        // Continuer vers le haut pour sortir de l'écran
        mettreAJourEtat(EtatAvion::DEPART);

        // Définir point de sortie très haut
        Position posActuelle = obtenirPosition();
        Position sortieEcran(posActuelle.x, -200.0f);  // Sortir par le haut
        ajouterTrajectoire(posActuelle, sortieEcran);

        stringstream ss;
        ss << "avion " << id_ << " decollage termine, en depart";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
    }
}

void Avion::phaseDepart() {
    // *** CORRECTION : Utiliser système de waypoints ***

    // Définir trajectoire sortie une seule fois
    bool doitCreerTrajectoire = false;

    {
        lock_guard<mutex> verrou(mutexWaypoints_);
        if (waypoints_.empty()) {
            doitCreerTrajectoire = true;
        }
    }

    if (doitCreerTrajectoire) {
        Position depart = obtenirPosition();
        Position sortieHaut(depart.x, -200.0f);  // Sortir par le haut

        ajouterTrajectoire(depart, sortieHaut);

        stringstream ss;
        ss << "avion " << id_ << " depart vers sortie ecran";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
    }

    // Suivre waypoints
    mettreAJourWaypoints(TEMPS_TRAME);

    // Vérifier si hors écran
    Position posActuelle = obtenirPosition();
    if (posActuelle.y < -150.0f || posActuelle.y > 750.0f ||
        posActuelle.x < -150.0f || posActuelle.x > 950.0f) {

        viderWaypoints();
        mettreAJourEtat(EtatAvion::PARTI);

        stringstream ss;
        ss << "avion " << id_ << " a quitte l'espace aerien";
        Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
    }
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


// définition des waypoints

void Avion::ajouterWaypoint(const Position& waypoint) {
    lock_guard<mutex> verrou(mutexWaypoints_);
    waypoints_.push(waypoint);
}

void Avion::ajouterTrajectoire(const Position& depart, const Position& arrivee) {
    lock_guard<mutex> verrou(mutexWaypoints_);

  while (!waypoints_.empty()) {
        waypoints_.pop();
    }

    float dx = arrivee.x - depart.x;
    float dy = arrivee.y - depart.y;
    
    bool horizontalDabord = abs(dx) > abs(dy);
    
    Position coin;
    if (horizontalDabord) {
    coin.x = arrivee.x;
        coin.y = depart.y;
    } else {
        coin.x = depart.x;
        coin.y = arrivee.y;
    }
    
    float distance1 = depart.distanceVers(coin);
    int nombreWaypoints1 = static_cast<int>(distance1 / 50.0f);
    if (nombreWaypoints1 < 1) nombreWaypoints1 = 1;
    
    for (int i = 1; i <= nombreWaypoints1; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(nombreWaypoints1);
        Position waypoint;
  waypoint.x = depart.x + (coin.x - depart.x) * t;
waypoint.y = depart.y + (coin.y - depart.y) * t;
        waypoints_.push(waypoint);
    }
    
    float distance2 = coin.distanceVers(arrivee);
    int nombreWaypoints2 = static_cast<int>(distance2 / 50.0f);
    if (nombreWaypoints2 < 1) nombreWaypoints2 = 1;
    
    for (int i = 1; i <= nombreWaypoints2; ++i) {
      float t = static_cast<float>(i) / static_cast<float>(nombreWaypoints2);
        Position waypoint;
        waypoint.x = coin.x + (arrivee.x - coin.x) * t;
        waypoint.y = coin.y + (arrivee.y - coin.y) * t;
     waypoints_.push(waypoint);
    }

    stringstream ss;
    ss << "avion " << id_ << " trajectoire en L definie " 
       << (nombreWaypoints1 + nombreWaypoints2) << " waypoints "
 << "(coin " << coin.x << "," << coin.y << ")";
  Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "DEBUG");
}


void Avion::viderWaypoints() {
    lock_guard<mutex> verrou(mutexWaypoints_);
    while (!waypoints_.empty()) {
        waypoints_.pop();
    }
}

void Avion::mettreAJourWaypoints(float dt) {
    Position waypointActuel;
  bool aWaypoint = false;
    bool waypointAtteint = false;

    {
        lock_guard<mutex> verrou(mutexWaypoints_);

        if (!waypoints_.empty()) {
    waypointActuel = waypoints_.front();
            aWaypoint = true;
 }
    }

    if (!aWaypoint) {
        return;
    }

    deplacerVers(waypointActuel, dt);

    if (aCibleAtteinte(waypointActuel, 3.0f)) {
        lock_guard<mutex> verrou(mutexWaypoints_);

        if (!waypoints_.empty()) {
  waypoints_.pop();
        waypointAtteint = true;

       if (!waypoints_.empty()) {
            stringstream ss;
        ss << "avion " << id_ << " waypoint atteint, reste " << waypoints_.size();
   Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "DEBUG");
 }
       else {
   stringstream ss;
     ss << "avion " << id_ << " dernier waypoint atteint";
    Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "INFO");
     }
        }
    }
}

void Avion::phaseEcrase() {
    // avion reste immobile
    stringstream ss;
    ss << "avion " << id_ << " reste ecrase position ("
        << position_.x << ", " << position_.y << ") bloque piste";
    Journaliseur::obtenirInstance().journaliser("Avion", ss.str(), "ERROR");

    // rester actif pour affichage mais arreter thread
}
