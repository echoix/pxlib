#include "config.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "px_intern.h"
#include "paradox-gsf.h"
#include "px_error.h"
#include "px_crypt.h"
#include "px_io.h"

/* px_stream_new() {{{
 *
 * Create a new stream
 */
pxstream_t *px_stream_new(pxdoc_t *pxdoc) {
	pxstream_t *pxs;
	if(pxdoc == NULL) {
		px_error(pxdoc, PX_RuntimeError, _("Did not pass a paradox database."));
		return NULL;
	}

	if(NULL == (pxs = pxdoc->malloc(pxdoc, sizeof(pxstream_t), _("Allocate memory for io stream.")))) {
		px_error(pxdoc, PX_MemoryError, _("Could not allocate memory for io stream."));
		return NULL;
	}
	
	return(pxs);
}
/* }}} */

#if PX_HAVE_GSF
/* px_stream_new_gsf() {{{
 *
 * Create a gsf stream
 */
pxstream_t *px_stream_new_gsf(pxdoc_t *pxdoc, int mode, int close, GsfInput *gsf) {
	pxstream_t *pxs;

	if(NULL == (pxs = px_stream_new(pxdoc)))
		return(NULL);
	
	pxs->type = pxfIOGsf;
	pxs->mode = mode;
	pxs->close = close;
	pxs->s.gsfin = gsf;

	pxs->read = px_gsfread;
	pxs->seek = px_gsfseek;
	pxs->tell = px_gsftell;
	pxs->write = px_gsfwrite;
	return(pxs);
}
/* }}} */
#endif

/* px_stream_new_file() {{{
 *
 * Create a file stream
 */
pxstream_t *px_stream_new_file(pxdoc_t *pxdoc, int mode, int close, FILE *fp) {
	pxstream_t *pxs;

	if(NULL == (pxs = px_stream_new(pxdoc)))
		return(NULL);

	pxs->type = pxfIOFile;
	pxs->mode = mode;
	pxs->close = close;
	pxs->s.fp = fp;

	pxs->read = px_fread;
	pxs->seek = px_fseek;
	pxs->tell = px_ftell;
	pxs->write = px_fwrite;
	return(pxs);
}
/* }}} */

/* Generic file access functions for .db and .px files */
/* px_read() {{{
 *
 * Generic read function doing decryption if needed.
 * It calls the read function from px_stream_t to actually get the
 * file data.
 */
size_t px_read(pxdoc_t *p, pxstream_t *dummy, size_t len, void *buffer) {
	size_t ret;
	long blocknr, blockpos, curpos, blocksize;
	pxhead_t *pxh;
	pxstream_t *pxs;

	pxh = p->px_head;
	pxs = p->px_stream;

	curpos = pxs->tell(p, pxs);
	if(pxh != NULL && curpos >= pxh->px_headersize) {
		blocksize = pxh->px_maxtablesize * 0x400;
		blocknr = ((curpos - pxh->px_headersize) / blocksize) + 1;
		blockpos = (curpos - pxh->px_headersize) % blocksize; 
//		fprintf(stderr, "reading from block %d:%d\n", blocknr, blockpos);
		if(blockpos+len > blocksize) {
			px_error(p, PX_RuntimeError, _("Trying to read data from file exceeds block boundry."));
			return(0);
		}
		if(p->curblock == NULL) {
			fprintf(stderr, "Allocate memory for cache block.\n");
			p->curblock = p->malloc(p, blocksize, _("Allocate memory for block cache."));
			if(p->curblock == NULL) {
				return(0);
			}
			
		}
		if(p->curblocknr != blocknr) {
			fprintf(stderr, "Read block %d into cache.\n", blocknr);
			pxs->seek(p, pxs, pxh->px_headersize + ((blocknr-1)*blocksize), SEEK_SET);
			pxs->read(p, pxs, blocksize, p->curblock);
			p->curblocknr = blocknr;
			if(pxh->px_encryption != 0) {
				fprintf(stderr, "Decrypting block %d\n", blocknr);
				px_decrypt_db_block(p->curblock, p->curblock, pxh->px_encryption, blocksize, blocknr);
			}
		} else {
//			fprintf(stderr, "block %d already in cache.\n", blocknr);
		}
		memcpy(buffer, p->curblock+blockpos, len);
		pxs->seek(p, pxs, curpos+len, SEEK_SET);
		ret = len;
	} else {
		ret = pxs->read(p, pxs, len, buffer);
	}
	return(ret);
}
/* }}} */

/* px_seek() {{{
 */
int px_seek(pxdoc_t *p, pxstream_t *dummy, long offset, int whence) {
	return(p->px_stream->seek(p, p->px_stream, offset, whence));
}
/* }}} */

/* px_tell() {{{
 */
long px_tell(pxdoc_t *p, pxstream_t *dummy) {
	return(p->px_stream->tell(p, p->px_stream));
}
/* }}} */

/* px_write() {{{
 */
size_t px_write(pxdoc_t *p, pxstream_t *dummy, size_t len, void *buffer) {
	size_t ret;
	long blocknr, blockpos, curpos, blocksize;
	pxhead_t *pxh;
	pxstream_t *pxs;

	pxh = p->px_head;
	pxs = p->px_stream;
	curpos = pxs->tell(p, pxs);
	if(pxh != NULL && curpos >= pxh->px_headersize) {
		blocksize = pxh->px_maxtablesize * 0x400;
		blocknr = ((curpos - pxh->px_headersize) / blocksize) + 1;
		blockpos = (curpos - pxh->px_headersize) % blocksize; 
//		fprintf(stderr, "writing to block %d:%d\n", blocknr, blockpos);
		if(blockpos+len > blocksize) {
			px_error(p, PX_RuntimeError, _("Trying to write data to file exceeds block boundry."));
			return(0);
		}
		if(p->curblock == NULL) {
			fprintf(stderr, "Allocate memory for cache block.\n");
			p->curblock = p->malloc(p, blocksize, _("Allocate memory for block cache."));
			if(p->curblock == NULL) {
				return(0);
			}
		}
		/* Write block to disk if the write operation modifies a new block.
		 * No need to write, if this is the first time a write operation
		 * modifies a block.
		 */
		if(p->curblocknr != blocknr && p->curblocknr != 0) {
			fprintf(stderr, "Write block %d from cache into file.\n", p->curblocknr);
			pxs->seek(p, pxs, pxh->px_headersize + ((p->curblocknr-1)*blocksize), SEEK_SET);
			if(pxh->px_encryption != 0) {
				fprintf(stderr, "Encrypting block %d\n", p->curblocknr);
				px_encrypt_db_block(p->curblock, p->curblock, pxh->px_encryption, blocksize, p->curblocknr);
			}
			pxs->write(p, pxs, blocksize, p->curblock);
			p->curblocknr = blocknr;
			memset(p->curblock, 0, blocksize);
		} else {
//			fprintf(stderr, "block %d already in cache.\n", blocknr);
		}
		p->curblockdirty = px_true;
		memcpy(p->curblock+blockpos, buffer, len);
		pxs->seek(p, pxs, curpos+len, SEEK_SET);
		ret = len;
	} else {
		ret = pxs->write(p, pxs, len, buffer);
	}
	return(ret);
}
/* }}} */

/* px_flush() {{{
 */
int px_flush(pxdoc_t *p, pxstream_t *dummy) {
	long blocksize;
	pxhead_t *pxh;
	pxstream_t *pxs;

	pxh = p->px_head;
	pxs = p->px_stream;
	if(pxh != NULL) {
		blocksize = pxh->px_maxtablesize * 0x400;
		if(p->curblockdirty) {
			fprintf(stderr, "Write block %d from cache into file.\n", p->curblocknr);
			pxs->seek(p, pxs, pxh->px_headersize + ((p->curblocknr-1)*blocksize), SEEK_SET);
			if(pxh->px_encryption != 0) {
				fprintf(stderr, "Encrypting block %d\n", p->curblocknr);
				px_encrypt_db_block(p->curblock, p->curblock, pxh->px_encryption, blocksize, p->curblocknr);
			}
			pxs->write(p, pxs, blocksize, p->curblock);
			p->curblockdirty = px_false;
		}
	}
	return(0);
}
/* }}} */

/* Generic file access functions for .mb */
/* px_mb_read() {{{
 *
 * Generic read function doing decryption if needed.
 * It calls the read function from px_stream_t to actually get the
 * file data.
 */
size_t px_mb_read(pxblob_t *p, pxstream_t *dummy, size_t len, void *buffer) {
	pxdoc_t *pxdoc;
	pxhead_t *pxh;
	pxstream_t *pxs;
	long pos;
	int ret;
	unsigned char *tmpbuf = NULL;
	unsigned int blockslen, blockoffset;

	pxdoc = p->pxdoc;
	pxh = pxdoc->px_head;
	pxs = p->mb_stream;

	if (pxh->px_encryption == 0)
		return pxs->read(pxdoc, pxs, len, buffer);

	pos = pxs->tell(pxdoc, pxs);
	if (pos < 0) {
		return pos;
	}

	blockoffset = (pos >> 12) << 12;
	blockslen = (((len - 1) >> 12) + 1) << 12;

	assert(blockslen >= len);
	assert(blockoffset <= (unsigned long)pos);
	assert((blockoffset+blockslen) >= (pos+len));

	ret = pxs->seek(pxdoc, pxs, blockoffset, SEEK_SET);
	if (ret < 0) {
		return ret;
	}

	tmpbuf = (unsigned char *) malloc(blockslen);
	if (tmpbuf == NULL) {
		return -ENOMEM;
	}

	ret = pxs->read(pxdoc, pxs, blockslen, tmpbuf);
	if (ret < 0) {
		free(tmpbuf);
		return ret;
	}
	px_decrypt_mb_block(tmpbuf, tmpbuf, pxh->px_encryption, blockslen);
	memcpy(buffer, tmpbuf + (pos - blockoffset), len);
	free(tmpbuf);

	ret = pxs->seek(pxdoc, pxs, pos + len, SEEK_SET);
	if (ret < 0) {
		return ret;
	}

	return len;

}
/* }}} */

/* px_mb_seek() {{{
 */
int px_mb_seek(pxblob_t *p, pxstream_t *dummy, long offset, int whence) {
	return(p->mb_stream->seek(p->pxdoc, p->mb_stream, offset, whence));
}
/* }}} */

/* px_mb_tell() {{{
 */
long px_mb_tell(pxblob_t *p, pxstream_t *dummy) {
	return(p->mb_stream->tell(p->pxdoc, p->mb_stream));
}
/* }}} */

/* px_mb_write() {{{
 */
size_t px_mb_write(pxblob_t *p, pxstream_t *dummy, size_t len, void *buffer) {
	return(p->mb_stream->write(p->pxdoc, p->mb_stream, len, buffer));
	pxdoc_t *pxdoc;
	pxhead_t *pxh;
	pxstream_t *pxs;
	long pos;
	int ret;
	unsigned char *tmpbuf = NULL;
	unsigned int blockslen, blockoffset;

	pxdoc = p->pxdoc;
	pxh = pxdoc->px_head;
	pxs = p->mb_stream;

	if (pxh->px_encryption == 0)
		return pxs->write(pxdoc, pxs, len, buffer);

	pos = pxs->tell(pxdoc, pxs);
	if (pos < 0) {
		return pos;
	}

	blockoffset = (pos >> 12) << 12;
	blockslen = (((len - 1) >> 12) + 1) << 12;

	assert(blockslen >= len);
	assert(blockoffset <= (unsigned long)pos);
	assert((blockoffset+blockslen) >= (pos+len));

	ret = pxs->seek(pxdoc, pxs, blockoffset, SEEK_SET);
	if (ret < 0) {
		return ret;
	}

	tmpbuf = (unsigned char *) malloc(blockslen);
	if (tmpbuf == NULL) {
		return -ENOMEM;
	}

	ret = pxs->read(pxdoc, pxs, blockslen, tmpbuf);
	if (ret < 0) {
		goto end;
	}

	px_decrypt_mb_block(tmpbuf, tmpbuf, pxh->px_encryption, blockslen);
	memcpy(tmpbuf + (pos - blockoffset), buffer, len);
	px_encrypt_mb_block(tmpbuf, tmpbuf, pxh->px_encryption, blockslen);

	ret = pxs->seek(pxdoc, pxs, blockoffset, SEEK_SET);
	if (ret < 0) {
		return ret;
	}
	ret = pxs->write(pxdoc, pxs, blockslen, tmpbuf);
	if (ret < 0) {
		goto end;
	}

	ret = pxs->seek(pxdoc, pxs, pos + len, SEEK_SET);
	if (ret < 0) {
		goto end;
	}

	ret = len;
end:
	free(tmpbuf);
	return ret;
}
/* }}} */

/* regular file pointer */
/* px_fread() {{{
 */
size_t px_fread(pxdoc_t *p, pxstream_t *stream, size_t len, void *buffer) {
	return(fread(buffer, len, 1, stream->s.fp));
}
/* }}} */

/* px_fseek() {{{
 */
int px_fseek(pxdoc_t *p, pxstream_t *stream, long offset, int whence) {
	return(fseek(stream->s.fp, offset, whence));
}
/* }}} */

/* px_ftell() {{{
 */
long px_ftell(pxdoc_t *p, pxstream_t *stream) {
	return(ftell(stream->s.fp));
}
/* }}} */

/* px_fwrite() {{{
 */
size_t px_fwrite(pxdoc_t *p, pxstream_t *stream, size_t len, void *buffer) {
	return(fwrite(buffer, len, 1, stream->s.fp));
}
/* }}} */

/* gsf */
#if PX_HAVE_GSF
/* px_gsfread() {{{
 */
size_t px_gsfread(pxdoc_t *p, pxstream_t *stream, size_t len, void *buffer) {
	return((int) gsf_input_read(stream->s.gsfin, len, buffer));
}
/* }}} */

/* px_gsfseek() {{{
 */
int px_gsfseek(pxdoc_t *p, pxstream_t *stream, long offset, int whence) {
	GSeekType gsfwhence = G_SEEK_SET;

	switch(whence) {
		case SEEK_CUR: gsfwhence = G_SEEK_CUR; break;
		case SEEK_END: gsfwhence = G_SEEK_END; break;
		case SEEK_SET: gsfwhence = G_SEEK_SET; break;
	}
	return(gsf_input_seek(stream->s.gsfin, offset, gsfwhence));
}
/* }}} */

/* px_gsftell() {{{
 */
long px_gsftell(pxdoc_t *p, pxstream_t *stream) {
	return(gsf_input_tell(stream->s.gsfin));
}
/* }}} */

/* px_gsfwrite() {{{
 */
size_t px_gsfwrite(pxdoc_t *p, pxstream_t *stream, size_t len, void *buffer) {
	return(gsf_output_write(stream->s.gsfout, len, buffer));
}
/* }}} */
#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
