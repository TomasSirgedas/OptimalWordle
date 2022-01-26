#include "Timer.h"
#include "WordleDictionary.h"

#include <iostream>
#include <functional>
#include <unordered_map>
#include <algorithm>
#include <bit>

using namespace std;

constexpr int MAX_NUM_GUESSES = 6;
constexpr int WORD_LEN = 5;
constexpr int CORRECT_ANSWER_BUCKET = 121;
constexpr double INF = 1e20;
string bucketToStr( int bucket )
{
   string ret;
   for ( int i = 0; i < WORD_LEN; i++, bucket /= 3 )
   {
      int bucketType = bucket % 3;
      ret += string("_#~")[bucketType];
   }
   reverse( ret.begin(), ret.end() );
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

// returns -1 if no guess is "optimal" (i.e. putting candidateWords into a separate bucket)
int optimalGuess( const vector<int>& candidateWords )
{
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
      return guess;
   skip:;
   }
   return -1;
}

vector<int> goodOrderForCandidateWords( vector<int> candidateWords )
{
   if ( candidateWords.size() < 50 )
      return candidateWords;

   int letterFrequency[26] = { 0 }; // but only count letter once in each word
   for ( int word : candidateWords )
   {
      int used = 0;
      for ( int i = 0; i < WORD_LEN; i++ )
      {
         int letter = g_allWords[word][i] - 'a';
         if ( used & (1<<letter) ) 
            continue;
         used |= 1<<letter;
         letterFrequency[letter]++;
      }
   }

   auto scoreForWord = [&letterFrequency]( int word ) { 
      int score = 0;
      for ( int i = 0; i < WORD_LEN; i++ )
         score += letterFrequency[g_allWords[word][i]-'a'];
      return score;
   };
   sort( candidateWords.begin(), candidateWords.end(), [&]( int a, int b ) { return scoreForWord( a ) > scoreForWord( b ); } );
   return candidateWords;
}

double calcScore( const vector<int>& candidateWords, int& bestGuess, int numGuessesLeft )
{
   if ( numGuessesLeft <= 0 )
      return INF;
   if ( candidateWords.size() == 1 )
      return 1;
   if ( numGuessesLeft <= 1 )
      return INF;
   if ( candidateWords.size() == 2 )
      return 1.5;

   double bestScore = INF;

   bool isTopLevel = candidateWords.size() == g_allWords.size();

   // optimization -- quickly check for an optimal guess (i.e. a guess that puts each candidate into a separate bucket)
   bestGuess = optimalGuess( candidateWords );
   if ( bestGuess >= 0 )
   {
      bestScore = 2 - 1./candidateWords.size();
   }
   else
   {
      if ( numGuessesLeft <= 2 ) // if optimal guess doesn't exist, we need at least 3 guesses
         return INF; 

      vector<int> candidateWordsInGoodGuessOrder = goodOrderForCandidateWords( candidateWords );

      //if ( numGuessesLeft == MAX_NUM_GUESSES )
      //   candidateWordsInGoodGuessOrder = { int( std::find( g_allWords.begin(), g_allWords.end(), "fjord" ) - g_allWords.begin() ) };

      // score each guess, and keep track which is best
      for ( int guess : candidateWordsInGoodGuessOrder )
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

         double score = 1;

         if ( candidateWords.size() > 64 )
         {
            unordered_map<int, vector<int>> wordsForBucket;
            for ( int candidateWord : candidateWords ) if ( guess != candidateWord )
            {
               int bucket = g_BucketForGuessTable.bucket( guess, candidateWord );
               wordsForBucket[bucket].push_back( candidateWord );
            } 

            for ( const auto& [bucket, remainingWords] : wordsForBucket )
            {
               double lowerBoundScoreForBucket = 2 - 1./remainingWords.size();
               int localBestGuess = remainingWords[0];
               double scoreForBucket = calcScore( remainingWords, localBestGuess, numGuessesLeft-1 );
               score += scoreForBucket * remainingWords.size() / candidateWords.size();
               lowerBoundScore += ( scoreForBucket - lowerBoundScoreForBucket ) * remainingWords.size() / candidateWords.size();
               if ( lowerBoundScore >= bestScore )
                  break;
            }
            if ( lowerBoundScore >= bestScore )
               continue;
         }
         else // optimization -- use bit flags (uint64_t) instead of vector<int>
         {
            unordered_map<int, uint64_t> wordsForBucket;
            for ( int candidateWordIndex = 0; candidateWordIndex < (int) candidateWords.size(); candidateWordIndex++ ) if ( guess != candidateWords[candidateWordIndex] )
            {
               int bucket = g_BucketForGuessTable.bucket( guess, candidateWords[candidateWordIndex] );
               wordsForBucket[bucket] |= 1LL << candidateWordIndex;
            }

            for ( const auto& [bucket, remainingWordsBits] : wordsForBucket )
            {
               int remainingWordsSize = popcount( remainingWordsBits );
               double lowerBoundScoreForBucket = 2 - 1./remainingWordsSize;

               vector<int> remainingWords;
               {
                  remainingWords.reserve( remainingWordsSize );
                  uint64_t r = remainingWordsBits;
                  while ( r )
                  {
                     int idx = countr_zero( r );
                     r &= ~(1LL << idx); // clear the bit
                     remainingWords.push_back( candidateWords[idx] );
                  }
               }
               int localBestGuess = remainingWords[0];
               double scoreForBucket = calcScore( remainingWords, localBestGuess, numGuessesLeft-1 );
               score += scoreForBucket * remainingWordsSize / candidateWords.size();
               lowerBoundScore += ( scoreForBucket - lowerBoundScoreForBucket ) * remainingWordsSize / candidateWords.size();
               if ( lowerBoundScore >= bestScore )
                  break;
            }
            if ( lowerBoundScore >= bestScore )
               continue;
         }

         //if ( isTopLevel )
         //{
         //   cout << g_allWords[guess] << " " << score << "    " << lowerBoundScore << endl;
         //}

         if ( score < bestScore )
         {
            bestScore = score;
            bestGuess = guess;
         }

         if ( bestScore < 2.0000001 )
            break; // nothing can be better, since we checked for optimal already
      }
   }


   //if ( isTopLevel )
   //{
   //   cout << g_allWords[bestGuess] << bestGuess << endl;
   //}

   return bestScore;
}

void searchWithIncreasingDictionarySizes()
{
   for ( int sz = 670; ; sz += 1 )
   {
      g_allWords = WordleDictionary::getWords( sz );

      vector<int> words;
      for ( int i = 0; i < (int) g_allWords.size(); i++ )
         words.push_back( i );

      g_BucketForGuessTable = BucketForGuessTable();

      Timer t;
      int bestGuess = words[0];
      double score = calcScore( words, bestGuess, MAX_NUM_GUESSES );
      cout << sz << "\t" << score << "\t" << t.elapsedTime() << "\t" << g_allWords[bestGuess] << endl;
   }
}

double doSearchTree( const vector<int>& candidateWords, const string& prefix, int guessesLeft )
{
   int guess = candidateWords[0]; 
   double score = calcScore( candidateWords, guess, guessesLeft );  
   if ( guess == -1 )
      throw "invalid guess";

   unordered_map<int, vector<int>> wordsForBucket;
   for ( int candidateWord : candidateWords )
   {
      int bucket = g_BucketForGuessTable.bucket( guess, candidateWord );
      wordsForBucket[bucket].push_back( candidateWord );
   }

   vector<pair<int, vector<int>>> sortedWordsForBucket;
   {
      for ( const auto& [bucket, remainingWords] : wordsForBucket )
         sortedWordsForBucket.push_back( {bucket, remainingWords} );
      sort( sortedWordsForBucket.begin(), sortedWordsForBucket.end(), []( const pair<int, vector<int>>& a, const pair<int, vector<int>>& b )
      {
         int scoreA = a.first == CORRECT_ANSWER_BUCKET ? 999999 : (int) a.second.size();
         int scoreB = b.first == CORRECT_ANSWER_BUCKET ? 999999 : (int) b.second.size();
         return scoreA > scoreB;
      } );
   }

   for ( const auto& [bucket, remainingWords] : sortedWordsForBucket /*wordsForBucket*/ )
   {
      //vector<string> v;
      //for ( int w : remainingWords )
      //   v.push_back( g_allWords[w] );

      //trace << g_allWords[guess] << " " << bucketToStr( bucket ) << " " << v << endl;

      //trace << prefix << g_allWords[guess] << " " << bucketToStr( bucket ) << endl;

      if ( bucket == CORRECT_ANSWER_BUCKET )
         cout << prefix << g_allWords[guess] + " " + bucketToStr( bucket )/* << "  [" << score << "]"*/ << endl;
      else
         doSearchTree( remainingWords, prefix + g_allWords[guess] + " " + bucketToStr( bucket ) + "  ", guessesLeft-1 );
   }

   return score;
}

void calcSearchTree( int dictionarySize )
{
   g_allWords = WordleDictionary::getWords( dictionarySize );
   g_BucketForGuessTable = BucketForGuessTable();

   vector<int> words;
   for ( int i = 0; i < (int) g_allWords.size(); i++ )
      words.push_back( i );

   Timer t;
   double score = doSearchTree( words, "", MAX_NUM_GUESSES );
   cout << "score = " << score << endl;
   cout << "time = " << t.elapsedTime() << endl;
}

int main()
{
   if ( calcBucketForGuess( "abcde", "abcde" ) != CORRECT_ANSWER_BUCKET )
      throw "CORRECT_ANSWER_BUCKET constant incorrect";

   //searchWithIncreasingDictionarySizes();
   calcSearchTree( 2315 );
      
   { cout << "(press <enter> to exit)" << endl; cin.get(); }
   return 0;
}