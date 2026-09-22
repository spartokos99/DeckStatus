// Single source for the application version: the CMake project declaration.
const fs=require('node:fs'), path=require('node:path');
const declaration=fs.readFileSync(path.resolve(__dirname,'..','CMakeLists.txt'),'utf8');
const match=/^project\(DeckStatus VERSION ([0-9]+\.[0-9]+\.[0-9]+)/m.exec(declaration);
if(!match) throw new Error('Could not read the project version from CMakeLists.txt');
module.exports=match[1];
