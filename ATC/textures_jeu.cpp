#include "textures_jeu.h"

void textures_jeu::charger() {
    // creez un dossier assets a cote de l executable et mettez y vos images
    // on essaie plusieurs chemins pour etre sur mdrr
    vector<string> chemins_base = { "assets/", "../ATC/assets/", "../../ATC/assets/", "../test/assets/" };
    
    for (const auto& base : chemins_base) {
        if (a_avion && a_carte && a_herbe && a_piste && a_tour) break; // tout est charge

        if (!a_avion && avion.loadFromFile(base + "plane.png")) { a_avion = true; avion.setSmooth(true); }
        if (!a_carte && carte.loadFromFile(base + "map.png")) { a_carte = true; carte.setSmooth(true); }
        if (!a_herbe && herbe.loadFromFile(base + "grass.png")) { a_herbe = true; herbe.setRepeated(true); }
        if (!a_piste && piste.loadFromFile(base + "pistecc.png")) { a_piste = true; }
        if (!a_tour && tour.loadFromFile(base + "tower.png")) { a_tour = true; tour.setSmooth(true); }
    }

    if (!a_avion) cerr << "erreur impossible de charger plane.png" << endl;
    if (!a_carte) cerr << "erreur impossible de charger map.png" << endl;
}
