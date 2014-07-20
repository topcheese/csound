/* 
 
 CachedButton.m:
 
 Copyright (C) 2011 Steven Yi
 
 This file is part of Csound for iOS.
 
 The Csound for iOS Library is free software; you can redistribute it
 and/or modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.   
 
 Csound is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with Csound; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 02111-1307 USA
 
 */
#import "CachedButton.h"

@implementation CachedButton

-(void)updateValueCache:(id)sender {
    cachedValue = 1;
    self.cacheDirty = YES;
}

-(id)init:(NSButton *)button channelName:(NSString *)channelName {
    if (self = [super init]) {
        self.channelName = channelName;
        self.button = button;
    }
    return self;
}


-(void)setup:(CsoundObj*)csoundObj {
    cachedValue = 0;
    self.cacheDirty = YES;
    channelPtr = [csoundObj getInputChannelPtr:self.channelName
                                   channelType:CSOUND_CONTROL_CHANNEL];
    [self.button setTarget:self];
    [self.button setAction:@selector(updateValueCache:)];
}


-(void)updateValuesToCsound {
    if (self.cacheDirty) {
        *channelPtr = cachedValue;
        
        self.cacheDirty = (cachedValue == 1);
        cachedValue = 0;
    }
}

-(void)cleanup {
    [self.button setTarget:nil];
    [self.button setAction:nil];
}


@end
