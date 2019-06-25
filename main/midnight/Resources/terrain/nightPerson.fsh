#ifdef GL_ES
precision mediump float;
#endif

varying vec2 v_texCoord;

vec4 m = vec4( 0.0, 0.0, 0.0, 0.0 ) ;

uniform vec4 p_left;
uniform vec4 p_right  ;
uniform float p_alpha  ;

void main()
{
    vec4 c = texture2D(CC_Texture0, v_texCoord).rgba;
       vec4 n;
    
//    if ( c.a <= 0.1 ) {
//        gl_FragColor = c ;
//        return;
//    }
    
    n = c;
    
    c.g = 1.0 - ((c.g + c.r + c.b) / 3.0);
    c.r = c.g;
    c.b = c.g;
    
    float p = c.g;
    
    if ( c.a > 0.1 ) {
    
    //if ( p == 0.0  )
    //    n = vec4(p_left.r, p_left.g, p_left.b, c.a) ;
    //else if ( p == 1.0 )
    //    n = vec4(p_right.r, p_right.g, p_right.b, c.a) ;
    //else
    {
        m.r = 1.0 - c.r ;
        m.g = 1.0 - c.g ;
        m.b = 1.0 - c.b ;
        m.a = c.a;

        n = vec4((p_right.r*c.r)+(p_left.r*m.r), (p_right.g*c.g)+(p_left.g*m.g), (p_right.b*c.b)+(p_left.b*m.b), m.a) ;
       // n = vec4(0,0,0,c.a);
    }

    n.a = n.a * p_alpha;
    }
    
    gl_FragColor = n ;
    
}

