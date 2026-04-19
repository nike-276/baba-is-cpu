#pragma once

#include "core/direction.hpp"
#include "core/kind.hpp"

#include <string>
#include <vector>

namespace baba::editor {

struct PaletteEntry {
    core::Kind      kind;
    bool            is_text;
    core::Direction default_facing{core::Direction::Right};
    std::string     display_name;
};

inline std::vector<PaletteEntry> default_palette() {
    using K = core::Kind;
    using D = core::Direction;

    std::vector<PaletteEntry> p;
    auto add = [&](K k, bool text, const char* name) {
        p.push_back({k, text, D::Right, name});
    };

    // Objects
    add(K::N_Baba,   false, "baba");
    add(K::N_Wall,   false, "wall");
    add(K::N_Flag,   false, "flag");
    add(K::N_Rock,   false, "rock");
    add(K::N_Water,  false, "water");
    add(K::N_Lava,   false, "lava");
    add(K::N_Skull,  false, "skull");
    add(K::N_Door,   false, "door");
    add(K::N_Key,    false, "key");
    add(K::N_Keke,   false, "keke");
    add(K::N_Fofo,   false, "fofo");
    add(K::N_Me,     false, "me");
    add(K::N_Box,    false, "box");
    add(K::N_Leaf,   false, "leaf");
    add(K::N_Cloud,  false, "cloud");
    add(K::N_Sun,    false, "sun");
    add(K::N_Moon,   false, "moon");
    add(K::N_Star,   false, "star");
    add(K::N_Planet, false, "planet");
    add(K::N_Bolt,   false, "bolt");
    add(K::N_Love,   false, "love");
    add(K::N_Bomb,   false, "bomb");
    add(K::N_Wind,   false, "wind");
    add(K::N_Track,  false, "track");
    add(K::N_Belt,     false, "belt");
    add(K::N_Algae,   false, "algae");   add(K::N_Arm,     false, "arm");
    add(K::N_Arrow,   false, "arrow");   add(K::N_Badbad,  false, "badbad");
    add(K::N_Banana,  false, "banana");  add(K::N_Bat,     false, "bat");
    add(K::N_Bean,    false, "bean");    add(K::N_Bed,     false, "bed");
    add(K::N_Bee,     false, "bee");     add(K::N_Bird,    false, "bird");
    add(K::N_Blob,    false, "blob");    add(K::N_Boat,    false, "boat");
    add(K::N_Boba,    false, "boba");    add(K::N_Bog,     false, "bog");
    add(K::N_Bone,    false, "bone");    add(K::N_Book,    false, "book");
    add(K::N_Bottle,  false, "bottle");  add(K::N_Brain,   false, "brain");
    add(K::N_Brick,   false, "brick");   add(K::N_Bubble,  false, "bubble");
    add(K::N_Bucket,  false, "bucket");  add(K::N_Bug,     false, "bug");
    add(K::N_Bunny,   false, "bunny");   add(K::N_Burger,  false, "burger");
    add(K::N_Cactus,  false, "cactus");  add(K::N_Cake,    false, "cake");
    add(K::N_Car,     false, "car");     add(K::N_Cart,    false, "cart");
    add(K::N_Cash,    false, "cash");    add(K::N_Cat,     false, "cat");
    add(K::N_Chair,   false, "chair");   add(K::N_Cheese,  false, "cheese");
    add(K::N_Chili,   false, "chili");   add(K::N_Circle,  false, "circle");
    add(K::N_Cliff,   false, "cliff");   add(K::N_Clock,   false, "clock");
    add(K::N_Cog,     false, "cog");     add(K::N_Crab,    false, "crab");
    add(K::N_Crystal, false, "crystal"); add(K::N_Cup,     false, "cup");
    add(K::N_Dog,     false, "dog");     add(K::N_Donut,   false, "donut");
    add(K::N_Dot,     false, "dot");     add(K::N_Drink,   false, "drink");
    add(K::N_Drum,    false, "drum");    add(K::N_Dust,    false, "dust");
    add(K::N_Ear,     false, "ear");     add(K::N_Egg,     false, "egg");
    add(K::N_Eye,     false, "eye");     add(K::N_Fence,   false, "fence");
    add(K::N_Fire,    false, "fire");    add(K::N_Fish,    false, "fish");
    add(K::N_Flower,  false, "flower");  add(K::N_Foliage, false, "foliage");
    add(K::N_Foot,    false, "foot");    add(K::N_Fort,    false, "fort");
    add(K::N_Fox,     false, "fox");     add(K::N_Frog,    false, "frog");
    add(K::N_Fruit,   false, "fruit");   add(K::N_Fungi,   false, "fungi");
    add(K::N_Fungus,  false, "fungus");  add(K::N_Gate,    false, "gate");
    add(K::N_Gem,     false, "gem");     add(K::N_Ghost,   false, "ghost");
    add(K::N_Grass,   false, "grass");   add(K::N_Guitar,  false, "guitar");
    add(K::N_Hand,    false, "hand");    add(K::N_Hedge,   false, "hedge");
    add(K::N_Hihat,   false, "hihat");   add(K::N_Hotdog,  false, "hotdog");
    add(K::N_House,   false, "house");   add(K::N_Husk,    false, "husk");
    add(K::N_Husks,   false, "husks");   add(K::N_Ice,     false, "ice");
    add(K::N_It,      false, "it");      add(K::N_Jelly,   false, "jelly");
    add(K::N_Jiji,    false, "jiji");    add(K::N_Knight,  false, "knight");
    add(K::N_Ladder,  false, "ladder");  add(K::N_Lamp,    false, "lamp");
    add(K::N_Lever,   false, "lever");   add(K::N_Lift,    false, "lift");
    add(K::N_Lily,    false, "lily");    add(K::N_Line,    false, "line");
    add(K::N_Lizard,  false, "lizard");  add(K::N_Lock,    false, "lock");
    add(K::N_Mirror,  false, "mirror");  add(K::N_Monitor, false, "monitor");
    add(K::N_Monster, false, "monster"); add(K::N_No,      false, "no");
    add(K::N_Nose,    false, "nose");    add(K::N_Orb,     false, "orb");
    add(K::N_Palm,    false, "palm");    add(K::N_Pants,   false, "pants");
    add(K::N_Paper,   false, "paper");   add(K::N_Pawn,    false, "pawn");
    add(K::N_Piano,   false, "piano");   add(K::N_Pillar,  false, "pillar");
    add(K::N_Pipe,    false, "pipe");    add(K::N_Pixel,   false, "pixel");
    add(K::N_Pizza,   false, "pizza");   add(K::N_Plane,   false, "plane");
    add(K::N_Plank,   false, "plank");   add(K::N_Potato,  false, "potato");
    add(K::N_Pumpkin, false, "pumpkin"); add(K::N_Reed,    false, "reed");
    add(K::N_Ring,    false, "ring");    add(K::N_Road,    false, "road");
    add(K::N_Robot,   false, "robot");   add(K::N_Rocket,  false, "rocket");
    add(K::N_Rose,    false, "rose");    add(K::N_Rubble,  false, "rubble");
    add(K::N_Sax,     false, "sax");     add(K::N_Scissors,false, "scissors");
    add(K::N_Seed,    false, "seed");    add(K::N_Shell,   false, "shell");
    add(K::N_Shirt,   false, "shirt");   add(K::N_Shovel,  false, "shovel");
    add(K::N_Sign,    false, "sign");    add(K::N_Snail,   false, "snail");
    add(K::N_Spike,   false, "spike");   add(K::N_Sprout,  false, "sprout");
    add(K::N_Square,  false, "square");  add(K::N_Statue,  false, "statue");
    add(K::N_Stick,   false, "stick");   add(K::N_Stump,   false, "stump");
    add(K::N_Sword,   false, "sword");   add(K::N_Table,   false, "table");
    add(K::N_Teeth,   false, "teeth");   add(K::N_TileObj, false, "tile");
    add(K::N_Tower,   false, "tower");   add(K::N_Train,   false, "train");
    add(K::N_Tree,    false, "tree");    add(K::N_Trees,   false, "trees");
    add(K::N_Triangle,false, "triangle");add(K::N_Trumpet, false, "trumpet");
    add(K::N_Turnip,  false, "turnip");  add(K::N_Turtle,  false, "turtle");
    add(K::N_Ufo,     false, "ufo");     add(K::N_Vase,    false, "vase");
    add(K::N_Vine,    false, "vine");    add(K::N_What,    false, "what");
    add(K::N_Worm,    false, "worm");    add(K::N_Yes,     false, "yes");

    // Text: nouns
    add(K::N_Baba,   true, "T:baba");
    add(K::N_Wall,   true, "T:wall");
    add(K::N_Flag,   true, "T:flag");
    add(K::N_Rock,   true, "T:rock");
    add(K::N_Water,  true, "T:water");
    add(K::N_Lava,   true, "T:lava");
    add(K::N_Skull,  true, "T:skull");
    add(K::N_Door,   true, "T:door");
    add(K::N_Key,    true, "T:key");
    add(K::N_Keke,   true, "T:keke");
    add(K::N_Fofo,   true, "T:fofo");
    add(K::N_Me,     true, "T:me");
    add(K::N_Box,    true, "T:box");
    add(K::N_Leaf,   true, "T:leaf");
    add(K::N_Cloud,  true, "T:cloud");
    add(K::N_Sun,    true, "T:sun");
    add(K::N_Moon,   true, "T:moon");
    add(K::N_Star,   true, "T:star");
    add(K::N_Planet, true, "T:planet");
    add(K::N_Bolt,   true, "T:bolt");
    add(K::N_Love,   true, "T:love");
    add(K::N_Bomb,   true, "T:bomb");
    add(K::N_Wind,   true, "T:wind");
    add(K::N_Track,   true, "T:track");
    add(K::N_Belt,    true, "T:belt");
    add(K::N_Algae,   true, "T:algae");   add(K::N_Arm,     true, "T:arm");
    add(K::N_Arrow,   true, "T:arrow");   add(K::N_Badbad,  true, "T:badbad");
    add(K::N_Banana,  true, "T:banana");  add(K::N_Bat,     true, "T:bat");
    add(K::N_Bean,    true, "T:bean");    add(K::N_Bed,     true, "T:bed");
    add(K::N_Bee,     true, "T:bee");     add(K::N_Bird,    true, "T:bird");
    add(K::N_Blob,    true, "T:blob");    add(K::N_Boat,    true, "T:boat");
    add(K::N_Boba,    true, "T:boba");    add(K::N_Bog,     true, "T:bog");
    add(K::N_Bone,    true, "T:bone");    add(K::N_Book,    true, "T:book");
    add(K::N_Bottle,  true, "T:bottle");  add(K::N_Brain,   true, "T:brain");
    add(K::N_Brick,   true, "T:brick");   add(K::N_Bubble,  true, "T:bubble");
    add(K::N_Bucket,  true, "T:bucket");  add(K::N_Bug,     true, "T:bug");
    add(K::N_Bunny,   true, "T:bunny");   add(K::N_Burger,  true, "T:burger");
    add(K::N_Cactus,  true, "T:cactus");  add(K::N_Cake,    true, "T:cake");
    add(K::N_Car,     true, "T:car");     add(K::N_Cart,    true, "T:cart");
    add(K::N_Cash,    true, "T:cash");    add(K::N_Cat,     true, "T:cat");
    add(K::N_Chair,   true, "T:chair");   add(K::N_Cheese,  true, "T:cheese");
    add(K::N_Chili,   true, "T:chili");   add(K::N_Circle,  true, "T:circle");
    add(K::N_Cliff,   true, "T:cliff");   add(K::N_Clock,   true, "T:clock");
    add(K::N_Cog,     true, "T:cog");     add(K::N_Crab,    true, "T:crab");
    add(K::N_Crystal, true, "T:crystal"); add(K::N_Cup,     true, "T:cup");
    add(K::N_Dog,     true, "T:dog");     add(K::N_Donut,   true, "T:donut");
    add(K::N_Dot,     true, "T:dot");     add(K::N_Drink,   true, "T:drink");
    add(K::N_Drum,    true, "T:drum");    add(K::N_Dust,    true, "T:dust");
    add(K::N_Ear,     true, "T:ear");     add(K::N_Egg,     true, "T:egg");
    add(K::N_Eye,     true, "T:eye");     add(K::N_Fence,   true, "T:fence");
    add(K::N_Fire,    true, "T:fire");    add(K::N_Fish,    true, "T:fish");
    add(K::N_Flower,  true, "T:flower");  add(K::N_Foliage, true, "T:foliage");
    add(K::N_Foot,    true, "T:foot");    add(K::N_Fort,    true, "T:fort");
    add(K::N_Fox,     true, "T:fox");     add(K::N_Frog,    true, "T:frog");
    add(K::N_Fruit,   true, "T:fruit");   add(K::N_Fungi,   true, "T:fungi");
    add(K::N_Fungus,  true, "T:fungus");  add(K::N_Gate,    true, "T:gate");
    add(K::N_Gem,     true, "T:gem");     add(K::N_Ghost,   true, "T:ghost");
    add(K::N_Grass,   true, "T:grass");   add(K::N_Guitar,  true, "T:guitar");
    add(K::N_Hand,    true, "T:hand");    add(K::N_Hedge,   true, "T:hedge");
    add(K::N_Hihat,   true, "T:hihat");   add(K::N_Hotdog,  true, "T:hotdog");
    add(K::N_House,   true, "T:house");   add(K::N_Husk,    true, "T:husk");
    add(K::N_Husks,   true, "T:husks");   add(K::N_Ice,     true, "T:ice");
    add(K::N_It,      true, "T:it");      add(K::N_Jelly,   true, "T:jelly");
    add(K::N_Jiji,    true, "T:jiji");    add(K::N_Knight,  true, "T:knight");
    add(K::N_Ladder,  true, "T:ladder");  add(K::N_Lamp,    true, "T:lamp");
    add(K::N_Lever,   true, "T:lever");   add(K::N_Lift,    true, "T:lift");
    add(K::N_Lily,    true, "T:lily");    add(K::N_Line,    true, "T:line");
    add(K::N_Lizard,  true, "T:lizard");  add(K::N_Lock,    true, "T:lock");
    add(K::N_Mirror,  true, "T:mirror");  add(K::N_Monitor, true, "T:monitor");
    add(K::N_Monster, true, "T:monster"); add(K::N_No,      true, "T:no");
    add(K::N_Nose,    true, "T:nose");    add(K::N_Orb,     true, "T:orb");
    add(K::N_Palm,    true, "T:palm");    add(K::N_Pants,   true, "T:pants");
    add(K::N_Paper,   true, "T:paper");   add(K::N_Pawn,    true, "T:pawn");
    add(K::N_Piano,   true, "T:piano");   add(K::N_Pillar,  true, "T:pillar");
    add(K::N_Pipe,    true, "T:pipe");    add(K::N_Pixel,   true, "T:pixel");
    add(K::N_Pizza,   true, "T:pizza");   add(K::N_Plane,   true, "T:plane");
    add(K::N_Plank,   true, "T:plank");   add(K::N_Potato,  true, "T:potato");
    add(K::N_Pumpkin, true, "T:pumpkin"); add(K::N_Reed,    true, "T:reed");
    add(K::N_Ring,    true, "T:ring");    add(K::N_Road,    true, "T:road");
    add(K::N_Robot,   true, "T:robot");   add(K::N_Rocket,  true, "T:rocket");
    add(K::N_Rose,    true, "T:rose");    add(K::N_Rubble,  true, "T:rubble");
    add(K::N_Sax,     true, "T:sax");     add(K::N_Scissors,true, "T:scissors");
    add(K::N_Seed,    true, "T:seed");    add(K::N_Shell,   true, "T:shell");
    add(K::N_Shirt,   true, "T:shirt");   add(K::N_Shovel,  true, "T:shovel");
    add(K::N_Sign,    true, "T:sign");    add(K::N_Snail,   true, "T:snail");
    add(K::N_Spike,   true, "T:spike");   add(K::N_Sprout,  true, "T:sprout");
    add(K::N_Square,  true, "T:square");  add(K::N_Statue,  true, "T:statue");
    add(K::N_Stick,   true, "T:stick");   add(K::N_Stump,   true, "T:stump");
    add(K::N_Sword,   true, "T:sword");   add(K::N_Table,   true, "T:table");
    add(K::N_Teeth,   true, "T:teeth");   add(K::N_TileObj, true, "T:tile");
    add(K::N_Tower,   true, "T:tower");   add(K::N_Train,   true, "T:train");
    add(K::N_Tree,    true, "T:tree");    add(K::N_Trees,   true, "T:trees");
    add(K::N_Triangle,true, "T:triangle");add(K::N_Trumpet, true, "T:trumpet");
    add(K::N_Turnip,  true, "T:turnip");  add(K::N_Turtle,  true, "T:turtle");
    add(K::N_Ufo,     true, "T:ufo");     add(K::N_Vase,    true, "T:vase");
    add(K::N_Vine,    true, "T:vine");    add(K::N_What,    true, "T:what");
    add(K::N_Worm,    true, "T:worm");    add(K::N_Yes,     true, "T:yes");

    // Text: operators
    add(K::O_Is,   true, "T:is");
    add(K::O_And,  true, "T:and");
    add(K::O_Not,  true, "T:not");
    add(K::O_On,   true, "T:on");
    add(K::O_Make, true, "T:make");
    add(K::O_Eat,  true, "T:eat");

    // Text: properties
    add(K::P_You,       true, "T:you");
    add(K::P_Push,      true, "T:push");
    add(K::P_Stop,      true, "T:stop");
    add(K::P_Win,       true, "T:win");
    add(K::P_Defeat,    true, "T:defeat");
    add(K::P_Sink,      true, "T:sink");
    add(K::P_Hot,       true, "T:hot");
    add(K::P_Melt,      true, "T:melt");
    add(K::P_Open,      true, "T:open");
    add(K::P_Shut,      true, "T:shut");
    add(K::P_Move,      true, "T:move");
    add(K::P_Weak,      true, "T:weak");
    add(K::P_Auto,      true, "T:auto");
    add(K::P_Fall,      true, "T:fall");
    add(K::P_Fallup,    true, "T:fallup");
    add(K::P_Fallleft,  true, "T:fallleft");
    add(K::P_Fallright, true, "T:fallright");
    add(K::P_Left,      true, "T:left");
    add(K::P_Right,     true, "T:right");
    add(K::P_Up,        true, "T:up");
    add(K::P_Down,      true, "T:down");
    add(K::P_Still,     true, "T:still");
    add(K::P_Shift,     true, "T:shift");
    add(K::P_Swap,      true, "T:swap");

    return p;
}

}  // namespace baba::editor
