# NetworksPG4
Creating a Network Pong Game :) 

## Included file names
netpong.cpp - contains both host and client handlers
Makefile - compliation 

## Example commands to run code 
User Host: ./netpong --host <portnum>
User Client: ./netpong student<num>.cse.nd.edu <portnum>

## Tasks
- [x] Set up basic communication between server and client 
  - [x] Initial host code 
  - [x] Initial client code 
- [x] Choose game difficulty level
  - [x] Host prompts user for difficulty level 
  - [x] Host sends level to client 
- [x] Add rounds to game
  - [x] Host prompts user for max number of rounds to play
  - [x] Host sends max round number to client
  - [x] Exit on both host and client when the max number of rounds are completed 
  - [x] Update score once a new round occurs
- [x] Synchronize gameplay
    - [x] Set up event-based (since we are using TCP) protocol that updates the other player
    - [x] Make sure scoring works properly 
    - [x] Make sure each user can only control their own paddle
    - [x] Check for edge cases
  
