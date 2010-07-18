/*****************************************************************************
 * JVLCCanvas.java: AWT Canvas containing VLC Video Output
 *****************************************************************************
 * 
 * Copyright (C) 1998-2006 the VideoLAN team
 * 
 * Author: Filippo Carone <filippo@carone.org>
 *
 * Created on 25-nov-2005
 *
 * $Id$
 * 
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 * 
 */

package org.videolan.jvlc;

import java.awt.Canvas;
import java.awt.Graphics;

public class JVLCCanvas extends Canvas {

    public native void paint(Graphics g);

    private final JVLC jvlc;
    
    /**
     * Default constructor. The canvas is set a dimension of 200x200
     */
    public JVLCCanvas() {
        jvlc = new JVLC();
        setSize(200, 200);
    }
    
    public JVLCCanvas(String[] args) {
    	jvlc = new JVLC(args);
    	setSize(200, 200);
    }
    
    /**
     * @param width The initial canvas width
     * @param height The initial canvas height
     */
    public JVLCCanvas(int width, int height) {
        jvlc = new JVLC();
        setSize(width, height);
    }
    
    public JVLCCanvas(String[] args, int width, int height) {
        jvlc = new JVLC(args);
        setSize(width, height);
    }

    public JVLCCanvas(JVLC jvlc) {
        this.jvlc = jvlc;
    }
    
    public JVLC getJVLC() {
        return jvlc;
    }

}
