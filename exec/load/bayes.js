/** $Id: bayes.js,v 1.1 2015/12/14 21:05:44 mcmlxxix Exp $
	Bayes Classifier ~
	Modified for Synchronet from: http://www.dusbabek.org/~garyd/bayes/bayes.js **/

function Bayes() {
	
	/* minimum probability for content to match a given class */
	this.THRESHOLD = 0.7;
	
	/* number of words to classify together */
	this.WORD_GROUPING = 1;

	/* remove common words from content before classifying? (recommended true, if WORD_GROUPING == 1) */
	this.STRIP_COMMON = true;
	
	/* There are two ways to compute joint probability. The easy way is to use the product of individual probabilities.
	The problem with this approach is that for large texts, probability tends towards extremely small values (think of
	0.01 * 0.01 * 0.01.	The solution is to use the sum of the logarithms of the probabilities.	No chance of underflow,
	but it has the disadvange of making the normalization that takes place at the end of all class calculations kind of
	hairy and unintuitive. */
 	this.USE_LOGS = true;

}

/* These words occur in the english language with enough frequency that they are considered noise.
 They will be removed from text that is considered.	The web site I found this list on claims that these 100 words
 occur with enough frequency to constitute 1/3 of typical english texts. */
Bayes.COMMON_WORDS = " the of an a to in is you that it he was for on are as with his they I "
					+ "at be this have from or one had by word but not what all were we when your can said "
					+ "there use an each which she do how their if will up other about out many then them these so "
					+ "some her would make like him into time has look two more write go see number no way could people "
					+ "my than first been call who its now find long down day did get come made may part "
					+ "this and ";
					
/* punctuation I want stripped from all text.	I'm taking the easy road at this point and assuming everything I come
across will be 7-bit ascii. */
Bayes.SYMBOLS = "!@#$%^&*()-+_=[]{}\\|;':\",.<>/?~`";

/* really fucking small number */
Bayes.VERY_UNLIKELY = 0.0000000001;

/* BEHHHHHHS! */
Bayes.prototype = {
	// stores information relating to how many times a word has recieved a specific classification.
	words: {},
	// stores information relating to how many times a classification has been assigned to a specific word.
	classes: {__length:0},
	
	/** public api **/
	
	isClass: function(content, cls) {
		var result = this.getClass(content);
		if(result[0] 
			&& result[0].classification.toLowerCase() == cls.toLowerCase()
			&& result[0].probability >= this.THRESHOLD)
			return true;
		else
			return false;
	},
	
	// classify a word or words
	getClass: function(content) {
		var tokens = this.scrub(content);
		var probs = [];
		var probSum = 0;
		
		for (var cls in this.classes) {
			if (cls == "__length") continue;
			var prob = this.probabilityOfClassGivenDocument(cls, tokens);
			probSum += prob;
			probs[probs.length] = {
				classification : cls,
				probability : prob,
				pc : this.probabilityOfClass(cls)
			};
		}
		
		// normalize
		// the log normalization conversion took me quite a bit of googling to figure out.	I'm still not sure how
		// accurate the math is.	Probabilities are adding up to 1 though, so I must be nearly right.
		var matchesPc = true;
		for (var i = 0; i < probs.length; i++) {
			if (this.USE_LOGS)
				probs[i].probability = 1 / (1 + Math.pow(Math.E, probSum-(2*probs[i].probability)));
			else if (probSum > 0)
				probs[i].probability = probs[i].probability / probSum;

			// in really bad cases, we may be given a text containing tokens that have never been seen before for any
			// classification.	In that cases, bayes craps out (because of the unseen seed value producing a very unlikely
			// probability) and returns a probability distribution that matches almost exactly the probability distribution of
			// p(C).	Recognize this bad case and shunt all probabilities to zero where they belong.
			var ratio = probs[i].probability / probs[i].pc;
			if (ratio < 0.95 || ratio > 1.05)
				matchesPc = false;
		}

		// maybe convert all (bogus) probabilities to zero.
		if (matchesPc) {
			for (var i = 0; i < probs.length; i++)
				probs[i].probability = 0;
		}

		// sort them highest first.
		probs.sort(function(a, b) {
			return b.probability - a.probability;
		});
		
		return probs;
	},
	
	// learn a word (or several words). This is where the statisical information is noted.
	teach: function(content, cls) {
		var tokens = this.scrub(content);
		var token;
		for (var i=0;i<tokens.length+this.WORD_GROUPING-1;i++) {
			if(this.WORD_GROUPING > 1) 
				token = tokens.slice(i,i+this.WORD_GROUPING);
			else
				token = tokens[i];
			this.sanitize(token, cls);
			this.words[token][cls]++;
			this.words[token].__length++;
			this.classes[cls][token]++;
			this.classes[cls].__length++;
			this.classes.__length++;
		}
	},

	// converts to lower case, removes punctuation and common words, converts to array.
	scrub: function(content) {
		
		// remove common words.
		function strip_common_words(tokens) {
			var newTokens = [];
			var ntPos = 0;
			for (var i = 0; i < tokens.length; i++) {
				var common = Bayes.COMMON_WORDS.match(" " + tokens[i] + " ");
				if (!common) {
					newTokens[ntPos++] = tokens[i];
				}
			}
			return newTokens;
		}

		// strip out punctuation.
		function strip_non_chars(content) {
			var res = "";
			for (var i=0;i<content.length;i++) {
				var ch = content.charAt(i);
				if (Bayes.SYMBOLS.indexOf(ch) < 0) {
					res += ch;
				}
			}
			return res;
		}

		content = content.toLowerCase();
		content = strip_non_chars(content); // remove punctuation, etc.
		content = content.replace(/\s+/g,' '); // replace white space runs with a single 0x20.
		content = content.split(" ");

		if(this.STRIP_COMMON)
			content = strip_common_words(content);
		
		return content;
	},

	/** internal shit **/
	
	// ensure that there are no null entries for a given word or class.
	sanitize: function(wrd, cls) {
		if (wrd != null) {
			if (typeof(this.words[wrd]) == "undefined")
				this.words[wrd] = {__length:0};
			if (typeof(this.words[wrd][cls]) == "undefined")
				this.words[wrd][cls] = 0;
		}

		if (typeof(this.classes[cls]) == "undefined")
			this.classes[cls] = {__length:0};
		if (typeof(this.classes[cls][wrd]) == "undefined")
			this.classes[cls][wrd] = 0.01;
	},

	// p(C|D). Take a set of unseen words and compute a probibility that the document belongs to the supplied class.
	probabilityOfClassGivenDocument: function(cls, tokens) {
		var prob = (this.USE_LOGS?0:1);
		var token;
		for (var i=0;i<tokens.length+this.WORD_GROUPING-1;i++) {
			if(this.WORD_GROUPING > 1) 
				token = tokens.slice(i,i+this.WORD_GROUPING);
			else
				token = tokens[i];
			var p = this.probabilityOfWordGivenClass(token, cls);
			if (this.USE_LOGS)
				prob += Math.log(p);
			else
				prob *= p;
		}
		if (this.USE_LOGS)
			prob += Math.log(this.probabilityOfClass(cls));
		else
			prob *= this.probabilityOfClass(cls);
		return prob;
	},

	// p(C)
	probabilityOfClass: function(cls) {
		this.sanitize(null, cls);
		var pc = this.classes[cls].__length / this.classes.__length;
		return pc;
	},

	// condtional probability. p(w|C).	probability a give word is classified as cls. uses empirical data.
	probabilityOfWordGivenClass: function(wrd, cls) {
		this.sanitize(wrd, cls);
		var prob = this.words[wrd][cls] / this.classes[cls].__length;
		// if a word has not been classified, don't use zero as the probability. It results in divide-by-zero later on.
		// instead, set it to an infinitesimal value indicating is very low probability. This skews the results, but not
		// too much.	This has the unfortunate side effect of introducing bogus probabilities when all tokens in a document
		// have never been seen before.	This unlikely event is handled later on when probabilities for all classes are
		// examined and ordered.
		if (prob == 0)
			return Bayes.VERY_UNLIKELY; // infinitesimal probability.
		else
			return prob;
	}

};




