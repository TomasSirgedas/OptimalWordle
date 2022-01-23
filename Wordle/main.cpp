#include "Timer.h"
#include "WordleDictionary.h"

#include <iostream>
#include <functional>
#include <unordered_map>
#include <algorithm>

using namespace std;


constexpr int WORD_LEN = 5;
string bucketToStr( int bucket )
{
   string ret;
   for ( int i = 0; i < WORD_LEN; i++ )
      ret += ( bucket & (1 << (i+WORD_LEN)) ) ? '#' : ( bucket & (1 << i) ) ? '*' : '.';
   return ret;
}

int calcBucketForGuess( const string& guess, const string& answer )
{
   int otherLetters[26] = { 0 };
   int buckets[WORD_LEN] = { 0 };
   for ( int i = 0; i < WORD_LEN; i++ )
      if ( guess[i] == answer[i] )
         buckets[i] = 1;
      else
         otherLetters[answer[i]-'a']++;
   for ( int i = 0; i < WORD_LEN; i++ )
      if ( guess[i] != answer[i] )
         if ( otherLetters[guess[i]-'a']-- > 0 )
            buckets[i] = 2;

   int ret = 0;
   for ( int i = 0; i < WORD_LEN; i++ )
      ret = ret * 3 + buckets[i];
   return ret;
}

vector<string> g_allWords;


class BucketForGuessTable
{
public:
   BucketForGuessTable()
   {
      int n = (int) g_allWords.size();
      m.resize( n );

      for ( int a = 0; a < n; a++ )
         for ( int b = 0; b < n; b++ )
            m[a].push_back( calcBucketForGuess( g_allWords[a], g_allWords[b] ) );
   }
   uint8_t bucket( int guess, int answer ) { return m[guess][answer]; }

public:
   vector<vector<uint8_t>> m;
} g_BucketForGuessTable;



double calcScore( const vector<int>& candidateWords )
{
   if ( candidateWords.size() == 1 )
      return 1;
   if ( candidateWords.size() == 2 )
      return 1.5;

   double bestScore = 99999;
   int bestGuess = -1;

   bool isTopLevel = candidateWords.size() == g_allWords.size();

   // optimization -- quickly check for an optimal guess (i.e. a guess that puts each candidate into a separate bucket)
   for ( int guess : candidateWords )
   {
      uint64_t bucketsUsed[4] = { 0 };
      for ( int candidateWord : candidateWords ) if ( guess != candidateWord )
      {
         uint8_t bucket = g_BucketForGuessTable.bucket( guess, candidateWord );
         if ( bucketsUsed[bucket/64] & (1LL<<(bucket&63)) )
            goto skip;
         bucketsUsed[bucket/64] |= (1LL<<(bucket&63));
      }
      return 2 - 1./candidateWords.size(); // all guesses land in separate buckets
      skip:;
   }


   for ( int guess : candidateWords )
   {
      uint64_t bucketsUsed[4] = { 0 };
      for ( int candidateWord : candidateWords ) if ( guess != candidateWord )
      {
         uint8_t bucket = g_BucketForGuessTable.bucket( guess, candidateWord );
         bucketsUsed[bucket/64] |= (1LL<<(bucket&63));
      }
      int numBucketsUsed = popcount( bucketsUsed[0] ) + popcount( bucketsUsed[1] ) + popcount( bucketsUsed[2] ) + popcount( bucketsUsed[3] );

      //double lowerBoundScore = 3 - ( numBucketsUsed + ( guessIsACandidate ? 2 : 0 ) ) / (double) candidateWords.size();
      double lowerBoundScore = 3 - ( numBucketsUsed + 2 ) / (double) candidateWords.size();

      if ( lowerBoundScore >= bestScore )
         continue;

      unordered_map<int, vector<int>> wordsForBucket;
      for ( int candidateWord : candidateWords ) if ( guess != candidateWord )
      {
         int bucket = g_BucketForGuessTable.bucket( guess, candidateWord );
         wordsForBucket[bucket].push_back( candidateWord );
      }

      if ( wordsForBucket.size() == 1 && wordsForBucket.begin()->second.size() == candidateWords.size() )
         continue;     

      double score = 1;
      for ( const auto& [bucket, remainingWords] : wordsForBucket )
      {
         double lowerBoundScoreForBucket = 2 - 1./remainingWords.size();
         double scoreForBucket = calcScore( remainingWords );
         score += scoreForBucket * remainingWords.size() / candidateWords.size();
         lowerBoundScore += ( scoreForBucket - lowerBoundScoreForBucket ) * remainingWords.size() / candidateWords.size();
         if ( lowerBoundScore >= bestScore )
            break;
      }
      if ( lowerBoundScore >= bestScore )
         continue;

      //if ( isTopLevel )
      //{
      //   cout << g_allWords[guess] << " " << score << "    " << lowerBoundScore << endl;
      //}

      if ( score < bestScore )
      {
         bestScore = score;
         bestGuess = guess;
      }

      if ( bestScore < 1.9999999 )
         break; // optimal 
      //if ( !guessIsACandidate && bestScore < 2.0000001 )
      //   break; // optimal 
   }


   //if ( isTopLevel )
   //{
   //   cout << g_allWords[bestGuess] << bestGuess << endl;
   //}

   return bestScore;
}

int main()
{
   for ( int sz = 700; ; sz += 20 )
   {
      g_allWords = WordleDictionary::getWords( sz );

      vector<int> words;
      for ( int i = 0; i < (int) g_allWords.size(); i++ )
         words.push_back( i );

      g_BucketForGuessTable = BucketForGuessTable();

      Timer t;
      double score = calcScore( words );
      cout << sz << "\t" << score << "\t" << t.elapsedTime() << endl;

      break;
   }
   return 0;
}

