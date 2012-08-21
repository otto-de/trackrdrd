/*
 * Copyright (c) 2012 UPLEX - Nils Goroll Systemoptimierung
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.</li>
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.</li>
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package de.otto.lhotse.track.rdr.testapp;

import org.apache.commons.lang3.RandomStringUtils;
import org.apache.commons.lang3.StringUtils;
import java.util.Random;

/**
 * Random data for use in the track reader test driver app.
 *
 * @author <a href="mailto:geoffrey.simmons@uplex.de">Geoffrey Simmons</a>
 *
 */

public class Data {
    
    private static final int STRLEN = 8;
    
    private static final String engines[] = {"Google", "Bing", "Yahoo"};
    
    private static final String types[] = {"order", "category", "search",
                                           "product", "user", "service"};
    
    private Random rand = new Random();

    public Data() {
    }

    public int nextInt(int i) {
        return rand.nextInt(i);
    }
    
    public String getString() {
        return RandomStringUtils.randomAlphabetic(STRLEN);
    }
    
    public int getColorDepth() {
        return (rand.nextInt(3) + 1) * 8;
    }
    
    public boolean getBoolean() {
        return rand.nextBoolean();
    }
    
    public String getHexInt() {
        return Integer.toHexString(rand.nextInt());
    }
    
    public String getGender() {
        if (rand.nextBoolean())
            return "W";
        return "M";
    }
    
    public String getEntryType() {
        if (rand.nextBoolean())
            return "SOFORT";
        return "POST";
    }
    
    public String getSearchTerms() {
        int n = rand.nextInt(5);
        String[] terms = new String[n];
        for (int i = 0; i < n; i++)
            terms[i] = RandomStringUtils.randomAlphabetic(rand.nextInt(STRLEN));
        return StringUtils.join(terms, "%20");
    }
    
    public String getSearchEngine() {
        return engines[rand.nextInt(engines.length)];
    }
    
    public String getPageCluster() {
        return types[rand.nextInt(types.length)];
    }
}
