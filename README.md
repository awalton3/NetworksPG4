# NetworksPG4
Creating a Network Pong Game :) 
## Tasks
- [x] Set up basic communication between server and client 
  - [x] Initial host code 
  - [x] Initial client code 
- [x] Choose game difficulty level
  - [x] Host prompts user for difficulty level 
  - [x] Host sends level to client 
- [ ] Add rounds to game
  - [x] Host prompts user for max number of rounds to play
  - [x] Host sends max round number to client
  - [x] Exit on both host and client when the max number of rounds are completed 
  - [x] Update score once a new round occurs
- [ ] Synchronize gameplay
    - [ ] Set up event-based (since we are using TCP) protocol that updates the other player
    - [~] Make sure scoring works properly (This sort of works but game is not in sync yet)
    - [ ] Make sure each user can only control their own paddle
    - [ ] Check for edge cases
  
